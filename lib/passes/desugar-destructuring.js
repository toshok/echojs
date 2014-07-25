/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import { startGenerator } from '../echo-util';
import { TransformPass } from '../node-visitor';
import * as b, { Identifier, ArrayPattern, ObjectPattern } from '../ast-builder';
import { reportError, reportWarning } from '../errors';

let gen = startGenerator();
let fresh = () => b.identifier(`%destruct_tmp${gen()}`);

// given an assignment { pattern } = id
// 
function createObjectPatternBindings (id, pattern, decls) {
    for (let prop of pattern.properties) {
        let memberexp = b.memberExpression(id, prop.key);
        
        decls.push(b.variableDeclarator(prop.key, memberexp));

        if (prop.value.type === Identifier) {
            // this happens with things like:  function foo ({w:i}) { }
            if (prop.value.name !== prop.key.name)
                throw new Error("not sure how to handle this case..");
        }
        else if (prop.value.type === ObjectPattern) {
            createObjectPatternBindings(memberexp, prop.value, decls);
        }
        else if (prop.value.type === ArrayPattern) {
            createArrayPatternBindings(memberexp, prop.value, decls);
        }
        else {
            throw new Error(`createObjectPatternBindings: prop.value.type = ${prop.value.type}`);
        }
    }
}

function createArrayPatternBindings (id, pattern, decls) {
    let el_num = 0;
    for (let el of pattern.elements) {
        let memberexp = b.memberExpression(id, b.literal(el_num), true);

        if (el.type === Identifier) {
            decls.push(b.variableDeclarator(el, memberexp));
        }
        else if (el.type === ObjectPattern) {
            let p_id = fresh();

            decls.push(b.variableDeclarator(p_id, memberexp));

            createObjectPatternBindings(p_id, el, decls);
        }
        else if (el.type === ArrayPattern) {
            let p_id = fresh();

            decls.push(b.variableDeclarator(p_id, memberexp));

            createArrayPatternBindings(p_id, el, decls);
        }
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
            if (p.type === ObjectPattern) {
                let p_id = fresh();
                new_params.push(p_id);
                let new_decl = b.letDeclaration();
                createObjectPatternBindings(p_id, p, new_decl.declarations);
                new_decls.push(new_decl);
            }
            else if (p.type === ArrayPattern) {
                let p_id = fresh();
                new_params.push(p_id);
                let new_decl = b.letDeclaration();
                createArrayPatternBindings(p_id, p, new_decl.declarations);
                new_decls.push(new_decl);
            }
            else if (p.type === Identifier) {
                // we just pass this along
                new_params.push(p);
            }
            else {
                throw new Error(`unhandled type of formal parameter in DesugarDestructuring ${p.type}`);
            }
        }

        n.body.body = new_decls.concat(n.body.body);
        n.params = new_params;
        return super(n);
    }

    visitVariableDeclaration (n) {
        let decls = [];

        for (let decl of n.declarations) {
            if (decl.id.type === ObjectPattern) {
                let obj_tmp_id = fresh();
                decls.push(b.variableDeclarator(obj_tmp_id, decl.init));
                createObjectPatternBindings(obj_tmp_id, decl.id, decls);
            }
            else if (decl.id.type === ArrayPattern) {
                // create a fresh tmp and declare it
                let array_tmp_id = fresh();
                decls.push(b.variableDeclarator(array_tmp_id, decl.init));
                createArrayPatternBindings(array_tmp_id, decl.id, decls);
            }
            else if (decl.id.type === Identifier) {
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
        if (n.left.type === ObjectPattern || n.left.type === ArrayPattern) {
            throw new Error("EJS doesn't support destructuring assignments yet (issue #16)");
            if (n.operator !== "=")
                reportError(Error, "cannot use destructuring with assignment operators other than '='", this.filename, n.loc);
        }
        else
            return super(n);
    }
}
