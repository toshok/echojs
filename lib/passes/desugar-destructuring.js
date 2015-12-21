/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import { startGenerator, intrinsic } from '../echo-util';
import { TransformPass } from '../node-visitor';
import * as b from '../ast-builder';
import * as escodegen from '../../external-deps/escodegen/escodegen-es6';
import { reportError, reportWarning } from '../errors';
import { Symbol_id, iterator_id, value_id, next_id, done_id,
         createIteratorWrapper_id,
         closeIteratorWrapper_id,
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
        
        if (prop.value.type === b.Identifier) {
            if (prop.computed) {
                bindings.push({ key: prop.value, value: memberexp });
                memberexp.computed = true;
            }
            else
                bindings.push({ key: prop.value, value: memberexp });
        }
        else if (prop.value.type === b.AssignmentPattern) {
            let default_id = fresh();
            bindings.push({ key: default_id, value: memberexp });
            bindings.push({ key: prop.value.left, value: b.conditionalExpression(default_id, default_id, prop.value.right) });
        }
        else if (prop.value.type === b.ObjectPattern) {
            bindings.push({ key: prop.key, value: memberexp });

            createObjectPatternBindings(memberexp, prop.value, bindings);
        }
        else if (prop.value.type === b.ArrayPattern) {
            bindings.push({ key: prop.key, value: memberexp });

            createArrayPatternBindingsUsingIterator(memberexp, prop.value, bindings);
        }
        else {
            throw new Error(`createObjectPatternBindings: prop.value.type = ${prop.value.type}`);
        }
    }
}

function createArrayPatternBindingsUsingIterator(id, pattern, bindings) {
    let seen_rest = false;

    // first off we create an iterator and wrapper for the rhs
    let iter_id = fresh();
    let wrapper_id = fresh();
    bindings.push({ key: iter_id, value: b.callExpression(b.memberExpression(id, Symbol_iterator, true), []), need_decl: true });
    bindings.push({ key: wrapper_id, value: intrinsic(createIteratorWrapper_id, [iter_id]), need_decl: true });

    for (let el of pattern.elements) {
        if (seen_rest)
            reportError(SyntaxError, "elements after rest element in array pattern", el.loc);

        if (el == null) {
            bindings.push({ key: fresh() /*unused*/, value: b.callExpression(b.memberExpression(wrapper_id, getNextValue_id), []) });
        }
        else if (el.type === b.Identifier) {
            bindings.push({ key: el, value: b.callExpression(b.memberExpression(wrapper_id, getNextValue_id), []) });
        }
        else if (el.type === b.AssignmentPattern) {
            let default_id = fresh();
            bindings.push({ key: default_id, value: b.callExpression(b.memberExpression(wrapper_id, getNextValue_id), []) });
            bindings.push({ key: el.left, value: b.conditionalExpression(default_id, default_id, el.right) });
        }
        else if (el.type === b.ObjectPattern) {
            let p_id = fresh();

            bindings.push({ key: p_id, value: b.callExpression(b.memberExpression(wrapper_id, getNextValue_id), []) });

            createObjectPatternBindings(p_id, el, bindings);
        }
        else if (el.type === b.ArrayPattern) {
            let p_id = fresh();

            bindings.push({ key: p_id, value: b.callExpression(b.memberExpression(wrapper_id, getNextValue_id), []) });

            createArrayPatternBindingsUsingIterator(p_id, el, bindings);
        }
        else if (el.type === b.RestElement) {
            bindings.push({ key: el.argument,  value: b.callExpression(b.memberExpression(wrapper_id, getRest_id), []) });
            seen_rest = true;
        }
        else
            throw new Error(`createArrayPatternBindingsUsingIterator ${el.type}`);
    }
}

export class DesugarDestructuring extends TransformPass {
    visitFunction (n) {
        return super.visitFunction(n);

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
                let bindings = [];
                createObjectPatternBindings(p_id, p, bindings);
                for (let binding of bindings) {
                    new_decls.push(b.letDeclaration(binding.key, binding.value));
                }
            }
            else if (ptype === b.ArrayPattern) {
                let p_id = fresh();
                new_params.push(p_id);
                let bindings = [];
                createArrayPatternBindingsUsingIterator(p_id, p, bindings);
                for (let binding of bindings) {
                    new_decls.push(b.letDeclaration(binding.key, binding.value));
                }
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
        let decl = n.declarations[0];

        if (decl.id.type === b.ObjectPattern) {
            let decls = [];
            let obj_tmp_id = fresh();
            let bindings = [];
            decls.push(b.variableDeclaration('let', obj_tmp_id, this.visit(decl.init)));
            createObjectPatternBindings(obj_tmp_id, decl.id, bindings);
            for (let binding of bindings) {
                decls.push(b.variableDeclaration(n.kind, binding.key, binding.value));
            }
            return decls;
        }
        else if (decl.id.type === b.ArrayPattern) {
            let decls = [];
            // create a fresh tmp and declare it
            let array_tmp_id = fresh();
            let bindings = [];
            decls.push(b.variableDeclaration('let', array_tmp_id, this.visit(decl.init)));
            createArrayPatternBindingsUsingIterator(array_tmp_id, decl.id, bindings);
            for (let binding of bindings) {
                decls.push(b.variableDeclaration(n.kind, binding.key, binding.value));
            }
            return decls;
        }
        else {
            return super.visitVariableDeclaration(n);
        }
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

            assignments.push(b.returnStatement(obj_tmp_id));

            return b.callExpression(b.functionExpression(null, [], b.blockStatement([tmp_decl, ...assignments])), []);
        }
        else
            return super.visitAssignmentExpression(n);
    }
}
