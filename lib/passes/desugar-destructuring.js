/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import { startGenerator, intrinsic } from "../echo-util";
import { TransformPass } from "../node-visitor";
import * as b from "../ast-builder";
import { reportError } from "../errors";
import {
    Symbol_id,
    iterator_id,
    createIteratorWrapper_id,
    getNextValue_id,
    getRest_id,
} from "../common-ids";

let gen = startGenerator();
let fresh = () => b.identifier(`%destruct_tmp${gen()}`);

// note: value-position identifiers must be fresh AST nodes per use (the
// EIR scope analysis resolves references in a node-keyed map), so the
// Symbol.iterator member expression is minted per call site
function symbolIterator() {
    return b.memberExpression(b.identifier(Symbol_id.name), iterator_id);
}

// bind `target` (an Identifier or a nested pattern) to `value`, applying
// the AssignmentPattern default `dflt` if present:
//   let %dt = value, target = %dt === undefined ? dflt : %dt;
function bindTarget(target, value, dflt, bindings) {
    if (dflt) {
        let dt = fresh();
        bindings.push({ key: dt, value: value, need_decl: true });
        value = b.conditionalExpression(
            b.binaryExpression(b.identifier(dt.name), "===", b.undefinedLit()),
            dflt,
            b.identifier(dt.name)
        );
    }

    if (target.type === b.Identifier) {
        bindings.push({ key: target, value: value });
        return;
    }

    // a nested pattern: land the (possibly defaulted) value in a temp and
    // recurse
    let pt = fresh();
    bindings.push({ key: pt, value: value, need_decl: true });
    if (target.type === b.ObjectPattern)
        createObjectPatternBindings(b.identifier(pt.name), target, bindings);
    else if (target.type === b.ArrayPattern)
        createArrayPatternBindingsUsingIterator(b.identifier(pt.name), target, bindings);
    else throw new Error(`bindTarget: target.type = ${target.type}`);
}

// given an assignment { pattern } = id
//
function createObjectPatternBindings(id, pattern, bindings) {
    for (let prop of pattern.properties) {
        let memberexp = b.memberExpression(id, prop.key);
        if (prop.computed) memberexp.computed = true;

        let target = prop.value;
        let dflt = null;
        if (target.type === b.AssignmentPattern) {
            dflt = target.right;
            target = target.left;
        }

        bindTarget(target, memberexp, dflt, bindings);
    }
}

function createArrayPatternBindingsUsingIterator(id, pattern, bindings) {
    let seen_spread = false;

    // first off we create an iterator and wrapper for the rhs
    let iter_id = fresh();
    let wrapper_id = fresh();
    bindings.push({
        key: iter_id,
        value: b.callExpression(b.memberExpression(id, symbolIterator(), true), []),
        need_decl: true,
    });
    bindings.push({
        key: wrapper_id,
        value: intrinsic(createIteratorWrapper_id, [iter_id]),
        need_decl: true,
    });

    let nextValue = () =>
        b.callExpression(b.memberExpression(b.identifier(wrapper_id.name), getNextValue_id), []);

    for (let el of pattern.elements) {
        if (seen_spread)
            reportError(SyntaxError, "elements after spread element in array pattern", el.loc);

        if (el == null) {
            bindings.push({ key: fresh() /*unused*/, value: nextValue() });
        } else if (el.type === b.SpreadElement || el.type === b.RestElement) {
            // declaration-position rests parse as SpreadElement,
            // assignment-position ones as RestElement
            bindings.push({
                key: el.argument,
                value: b.callExpression(
                    b.memberExpression(b.identifier(wrapper_id.name), getRest_id),
                    []
                ),
            });
            seen_spread = true;
        } else {
            let target = el;
            let dflt = null;
            if (target.type === b.AssignmentPattern) {
                dflt = target.right;
                target = target.left;
            }
            bindTarget(target, nextValue(), dflt, bindings);
        }
    }
}

export class DesugarDestructuring extends TransformPass {
    // don't touch a for-of's binding: DesugarForOf runs later and rewrites
    // it into a normal let declaration inside the loop body, which the
    // second DesugarDestructuring pass (after DesugarForOf) desugars.
    // visiting it here would split the pattern into multiple declarators,
    // of which DesugarForOf only keeps the first.
    visitForOf(n) {
        n.right = this.visit(n.right);
        n.body = this.visit(n.body);
        return n;
    }

    visitFunction(n) {
        // we visit the formal parameters directly, rewriting
        // them as tmp arg names and adding 'let' decls for the
        // pattern identifiers at the top of the function's
        // body.
        let new_params = [];
        let new_decls = [];
        for (let p of n.params) {
            let ptype = p.type;
            if (ptype === b.ObjectPattern) {
                let p_id = fresh();
                new_params.push(p_id);
                let new_decl = b.letDeclaration();
                let bindings = [];
                createObjectPatternBindings(p_id, p, bindings);
                for (let binding of bindings) {
                    new_decl.declarations.push(b.variableDeclarator(binding.key, binding.value));
                }
                new_decls.push(new_decl);
            } else if (ptype === b.ArrayPattern) {
                let p_id = fresh();
                new_params.push(p_id);
                let new_decl = b.letDeclaration();
                let bindings = [];
                createArrayPatternBindingsUsingIterator(p_id, p, bindings);
                for (let binding of bindings) {
                    new_decl.declarations.push(b.variableDeclarator(binding.key, binding.value));
                }
                new_decls.push(new_decl);
            } else if (ptype === b.Identifier) {
                // we just pass this along
                new_params.push(p);
            } else if (ptype === b.RestElement && p.argument.type === b.Identifier) {
                // this pass runs pre-EIR now, BEFORE DesugarRestParameters:
                // a trailing ...rest stays in place (EIR handles it
                // natively; the legacy rest pass strips it later)
                new_params.push(p);
            } else {
                throw new Error(
                    `unhandled type of formal parameter in DesugarDestructuring ${ptype}`
                );
            }
        }

        n.body.body = new_decls.concat(n.body.body);
        n.params = new_params;
        n.body = this.visit(n.body);
        return n;
    }

    visitVariableDeclaration(n) {
        let decls = [];

        for (let decl of n.declarations) {
            if (decl.id.type === b.ObjectPattern) {
                let obj_tmp_id = fresh();
                let bindings = [];
                decls.push(b.variableDeclarator(obj_tmp_id, this.visit(decl.init)));
                createObjectPatternBindings(obj_tmp_id, decl.id, bindings);
                for (let binding of bindings) {
                    decls.push(b.variableDeclarator(binding.key, binding.value));
                }
            } else if (decl.id.type === b.ArrayPattern) {
                // create a fresh tmp and declare it
                let array_tmp_id = fresh();
                let bindings = [];
                decls.push(b.variableDeclarator(array_tmp_id, this.visit(decl.init)));
                createArrayPatternBindingsUsingIterator(array_tmp_id, decl.id, bindings);
                for (let binding of bindings) {
                    decls.push(b.variableDeclarator(binding.key, binding.value));
                }
            } else if (decl.id.type === b.Identifier) {
                decl.init = this.visit(decl.init);
                decls.push(decl);
            } else {
                reportError(
                    Error,
                    `unhandled type of variable declaration in DesugarDestructuring ${decl.id.type}`,
                    this.filename,
                    n.loc
                );
            }
        }
        n.declarations = decls;
        return n;
    }

    visitAssignmentExpression(n) {
        if (n.left.type === b.ObjectPattern || n.left.type === b.ArrayPattern) {
            if (n.operator !== "=")
                reportError(
                    Error,
                    "cannot use destructuring with assignment operators other than =",
                    this.filename,
                    n.loc
                );

            let obj_tmp_id = fresh();
            let tmp_decl = b.letDeclaration(obj_tmp_id, this.visit(n.right));

            let assignments = [];
            let bindings = [];
            if (n.left.type === b.ObjectPattern)
                createObjectPatternBindings(obj_tmp_id, n.left, bindings);
            else createArrayPatternBindingsUsingIterator(obj_tmp_id, n.left, bindings);

            for (let binding of bindings) {
                if (binding.need_decl) {
                    assignments.push(b.letDeclaration(binding.key, binding.value));
                } else {
                    assignments.push(
                        b.expressionStatement(
                            b.assignmentExpression(binding.key, "=", binding.value)
                        )
                    );
                }
            }

            assignments.push(b.returnStatement(obj_tmp_id));

            return b.callExpression(
                b.functionExpression(null, [], b.blockStatement([tmp_decl, ...assignments])),
                []
            );
        } else return super.visitAssignmentExpression(n);
    }
}
