/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import { startGenerator, intrinsic } from '../echo-util';
import { TransformPass } from '../node-visitor';
import * as b from '../ast-builder';
import { reportError, reportWarning } from '../errors';
import { Symbol_id, iterator_id, value_id, next_id, done_id,
         createIteratorWrapper_id,
         getNextValue_id,
         getRest_id } from '../common-ids';

let gen = startGenerator();
let fresh = () => b.identifier(`%destruct_tmp${gen()}`);

let Symbol_iterator = b.memberExpression(Symbol_id, iterator_id);

// given an assignment { pattern } = id
// 
function createObjectPatternBindings (id, pattern, bindings) {
    for (let prop of pattern.properties) {
        let memberexp = b.memberExpression(id, prop.key);
        
        bindings.push({ key: prop.key, value: memberexp });

        if (prop.value.type === b.Identifier) {
            // this happens with things like:  function foo ({w:i}) { }
            if (prop.value.name !== prop.key.name)
                throw new Error("not sure how to handle this case..");
        }
        else if (prop.value.type === b.ObjectPattern) {
            createObjectPatternBindings(memberexp, prop.value, bindings);
        }
        else if (prop.value.type === b.ArrayPattern) {
            createArrayPatternBindingsUsingIterator(memberexp, prop.value, bindings);
        }
        else {
            throw new Error(`createObjectPatternBindings: prop.value.type = ${prop.value.type}`);
        }
    }
}

function createArrayPatternBindingsUsingIterator(id, pattern, bindings) {
    let seen_spread = false;

    // first off we create an iterator and wrapper for the rhs
    let iter_id = fresh();
    let wrapper_id = fresh();
    bindings.push({ key: iter_id, value: b.callExpression(b.memberExpression(id, Symbol_iterator, true), []), need_decl: true });
    bindings.push({ key: wrapper_id, value: intrinsic(createIteratorWrapper_id, [iter_id]), need_decl: true });

    for (let el of pattern.elements) {
        if (seen_spread)
            reportError(SyntaxError, "elements after spread element in array pattern", el.loc);

        if (el == null) {
            bindings.push({ key: fresh() /*unused*/, value: b.callExpression(b.memberExpression(wrapper_id, getNextValue_id), []) });
        }
        else if (el.type == b.Identifier) {
            bindings.push({ key: el, value: b.callExpression(b.memberExpression(wrapper_id, getNextValue_id), []) });
        }
        else if (el.type == b.ObjectPattern) {
            let p_id = fresh();

            bindings.push({ key: p_id, value: b.callExpression(b.memberExpression(wrapper_id, getNextValue_id), []) });

            createObjectPatternBindings(p_id, el, bindings);
        }
        else if (el.type === b.ArrayPattern) {
            let p_id = fresh();

            bindings.push({ key: p_id, value: b.callExpression(b.memberExpression(wrapper_id, getNextValue_id), []) });

            createArrayPatternBindingsUsingIterator(p_id, el, bindings);
        }
        else if (el.type === b.SpreadElement) {
            bindings.push({ key: el.argument,  value: b.callExpression(b.memberExpression(wrapper_id, getRest_id), []) });
            seen_spread = true;
        }
        else
            throw new Error(`createArrayPatternBindingsUsingIterator ${el.type}`);
    }
}

function createArrayPatternBindings (id, pattern, bindings) {
    let el_num = 0;
    let seen_spread = false;
    for (let el of pattern.elements) {
        let memberexp = b.memberExpression(id, b.literal(el_num), true);

        if (seen_spread)
            reportError(SyntaxError, "elements after spread element in array pattern", el.loc);

        if (el === null) {
            // happens when there's a hole in the array pattern, like var [a,,b] = [1,2,3];
            // we just skip that index and continue
            el_num += 1;
            continue;
        }

        if (el.type === b.Identifier) {
            bindings.push({ key: el, value: memberexp });
        }
        else if (el.type === b.ObjectPattern) {
            let p_id = fresh();

            bindings.push({ key: p_id, value: memberexp });

            createObjectPatternBindings(p_id, el, bindings);
        }
        else if (el.type === b.ArrayPattern) {
            let p_id = fresh();

            bindings.push({ key: p_id, value: memberexp });

            createArrayPatternBindings(p_id, el, bindings);
        }
        else if (el.type === b.SpreadElement) {
            bindings.push({ key: el.argument,  value: b.callExpression(b.memberExpression(id, b.identifier("slice"))) });
            seen_spread = true;
        }
        else
            throw new Error(`createArrayPatternBindings ${el.type}`);
        el_num += 1;
    }
}


export class DesugarDestructuring extends TransformPass {
    visitFunction (n) {
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
            }
            else if (ptype === b.ArrayPattern) {
                let p_id = fresh();
                new_params.push(p_id);
                let new_decl = b.letDeclaration();
                let bindings = [];
                createArrayPatternBindingsUsingIterator(p_id, p, bindings);
                for (let binding of bindings) {
                    new_decl.declarations.push(b.variableDeclarator(binding.key, binding.value));
                }
                new_decls.push(new_decl);
            }
            else if (ptype === b.Identifier) {
                // we just pass this along
                new_params.push(p);
            }
            else {
                throw new Error(`unhandled type of formal parameter in DesugarDestructuring ${ptype}`);
            }
        }

        n.body.body = new_decls.concat(n.body.body);
        n.params = new_params;
        n.body = this.visit(n.body);
        return n;
    }

    visitVariableDeclaration (n) {
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
            }
            else if (decl.id.type === b.ArrayPattern) {
                // create a fresh tmp and declare it
                let array_tmp_id = fresh();
                let bindings = [];
                decls.push(b.variableDeclarator(array_tmp_id, this.visit(decl.init)));
                createArrayPatternBindingsUsingIterator(array_tmp_id, decl.id, bindings);
                for (let binding of bindings) {
                    decls.push(b.variableDeclarator(binding.key, binding.value));
                }
            }
            else if (decl.id.type === b.Identifier) {
                decl.init = this.visit(decl.init);
                decls.push(decl);
            }
            else {
                reportError(Error, `unhandled type of variable declaration in DesugarDestructuring ${decl.id.type}`, this.filename, n.loc);
            }
        }
        n.declarations = decls;
        return n;
    }

    visitAssignmentExpression (n) {
        if (n.left.type === b.ObjectPattern || n.left.type === b.ArrayPattern) {
            if (n.operator !== "=")
                reportError(Error, "cannot use destructuring with assignment operators other than '='", this.filename, n.loc);

            let obj_tmp_id = fresh();
            let tmp_decl = b.letDeclaration(obj_tmp_id, this.visit(n.right));

            let assignments = [];
            let bindings = [];
            if (n.left.type === b.ObjectPattern)
                createObjectPatternBindings(obj_tmp_id, n.left, bindings);
            else
                createArrayPatternBindingsUsingIterator(obj_tmp_id, n.left, bindings);

            for (let binding of bindings) {
                if (binding.need_decl) {
                    assignments.push(b.letDeclaration(binding.key, binding.value));
                }
                else {
                    assignments.push(b.expressionStatement(b.assignmentExpression(binding.key, '=', binding.value)));
                }
            }

            return b.callExpression(b.functionExpression(null, [], b.blockStatement([tmp_decl, ...assignments])), []);
        }
        else
            return super.visitAssignmentExpression(n);
    }
}
