/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
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
module b from '../ast-builder';
import { FunctionDeclaration, FunctionExpression } from '../ast-builder';
import { startGenerator } from '../echo-util';
import { reportError, reportWarning } from '../errors';

function definesThis(n) {
    return n.type === FunctionDeclaration || n.type === FunctionExpression;
}

export class DesugarArrowFunctions extends TransformPass {
    
    constructor (options) {
        super(options);
        this.mapping = [];
        this.thisGen = startGenerator();
    }

    visitArrowFunctionExpression (n) {
        if (n.expression) {
            n.body = b.blockStatement([b.returnStatement(n.body)]);
            n.expression = false;
        }
        n = this.visitFunction(n);
        n.type = FunctionExpression;
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
                if (topfunc === m) return n;

                if (m.id) return b.identifier(m.id);

                m.id = `_this_${this.thisGen()}`;

                m.prepend = b.letDeclaration(b.identifier(m.id), b.thisExpression());
                
                return b.identifier(m.id);
            }
        }

        reportError(SyntaxError, "no binding for 'this' available for arrow function", this.filename, n.loc);
    }
    
    visitFunction (n) {
        this.mapping.unshift({ func: n, id: null });
        n = super(n);
        let m = this.mapping.shift();
        if (m.prepend)
            n.body.body.unshift(m.prepend);
        return n;
    }
}
