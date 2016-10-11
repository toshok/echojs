/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */
// this pass converts all arrow functions to normal anonymous function
// expressions with a closed-over 'this'
//
// take the following:
//
// function foo() {
//   let mapper = (arr) => {
//     arr.map (el => el * this.x);
//   };
// }
//
// This will be compiled to:
//
// function foo() {
//   let _this_010 = this;
//   let mapper = function (arr) {
//     arr.map (function (el) { return el * _this_010.x; });
//   };
// }
//
// and the usual closure conversion stuff will make sure the bindings
// exists in the closure env as usual.
// 

import { TransformPass } from '../node-visitor';
import * as b from '../ast-builder';
import { startGenerator } from '../echo-util';
import { reportError } from '../errors';

function definesThis(n) {
    return n.type === b.FunctionDeclaration || n.type === b.FunctionExpression;
}

export class DesugarArrowFunctions extends TransformPass {
    
    constructor (options) {
        super(options);
        this.mapping = [];
        this.thisGen = startGenerator();
    }

    visitArrowFunctionExpression (n) {
        if (n.expression) {
            n.body = b.blockStatement([b.returnStatement(n.body)], n.body.loc);
            n.expression = false;
        }
        n = this.visitFunction(n);
        n.type = b.FunctionExpression;
        return n;
    }

    visitThisExpression (n) {
        if (this.mapping.length === 0) {
            // a 'this' at toplevel.  not possible in ejs, since we wrap everything in toplevel functions
            return b.undefinedLit();
        }

        let topfunc = this.mapping[0].func;

        for (let m of this.mapping) {
            if (definesThis(m.func)) {
                // if we're already on top, just return the existing thisExpression
                if (topfunc === m.func) return n;

                if (m.this_id) return b.identifier(m.this_id);

                m.this_id = `_this_${this.thisGen()}`;

                m.prepend = b.letDeclaration(b.identifier(m.this_id), b.thisExpression());
                
                return b.identifier(m.this_id);
            }
        }

        reportError(SyntaxError, 'no binding for "this" available for arrow function', this.filename, n.loc);
    }

    visitIdentifier (n) {
        if (n.name !== 'arguments')
            return super.visitIdentifier(n);

        if (this.mapping.length > 0) {
            let topfunc = this.mapping[0].func;

            for (let m of this.mapping) {
                if (definesThis(m.func)) {
                    // if we're already on top, just return the existing thisExpression
                    if (topfunc === m.func) return n;

                    if (m.arguments_id) return b.identifier(m.arguments_id);

                    m.arguments_id = `_arguments_${this.thisGen()}`;

                    m.prepend = b.letDeclaration(b.identifier(m.arguments_id), n);
                
                    return b.identifier(m.arguments_id);
                }
            }

            reportError(SyntaxError, 'no binding for "arguments" available for arrow function', this.filename, n.loc);
        }
    }
    
    visitFunction (n) {
        this.mapping.unshift({ func: n, id: null });
        n = super.visitFunction(n);
        let m = this.mapping.shift();
        if (m.prepend)
            n.body.body.unshift(m.prepend);
        return n;
    }
}
