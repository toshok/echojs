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

import { TransformPass } from "../node-visitor";
import * as b from "../ast-builder";
import { startGenerator, is_intrinsic } from "../echo-util";
import { reportError } from "../errors";

function definesThis(n) {
    return n.type === b.FunctionDeclaration || n.type === b.FunctionExpression;
}

export class DesugarArrowFunctions extends TransformPass {
    constructor(options) {
        super(options);
        this.mapping = [];
        this.thisGen = startGenerator();
    }

    visitArrowFunctionExpression(n) {
        if (n.expression) {
            n.body = b.blockStatement([b.returnStatement(n.body)], n.body.loc);
            n.expression = false;
        }
        n = this.visitFunction(n);
        n.type = b.FunctionExpression;
        return n;
    }

    visitThisExpression(n) {
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

                m.prepends.push(b.letDeclaration(b.identifier(m.this_id), b.thisExpression()));

                return b.identifier(m.this_id);
            }
        }

        reportError(
            SyntaxError,
            'no binding for "this" available for arrow function',
            this.filename,
            n.loc
        );
    }

    visitIdentifier(n) {
        if (n.name !== "arguments") return super.visitIdentifier(n);

        if (this.mapping.length > 0) {
            let topfunc = this.mapping[0].func;

            for (let m of this.mapping) {
                if (definesThis(m.func)) {
                    // if we're already on top, just return the existing thisExpression
                    if (topfunc === m.func) return n;

                    if (m.arguments_id) return b.identifier(m.arguments_id);

                    m.arguments_id = `_arguments_${this.thisGen()}`;

                    m.prepends.push(b.letDeclaration(b.identifier(m.arguments_id), n));

                    return b.identifier(m.arguments_id);
                }
            }

            reportError(
                SyntaxError,
                'no binding for "arguments" available for arrow function',
                this.filename,
                n.loc
            );
        }
    }

    visitFunction(n) {
        // prepends is a list: an arrow using BOTH `this` and `arguments`
        // needs two declarations (a single .prepend slot lost one)
        this.mapping.unshift({ func: n, id: null, prepends: [] });
        n = super.visitFunction(n);
        let m = this.mapping.shift();
        if (m.prepends.length > 0) {
            n.body.body = m.prepends.concat(n.body.body);
            // a derived constructor's `this` only exists once super() has
            // run: the snapshot at function top reads undefined, so
            // re-snapshot after any top-level super call.  (arrows created
            // BEFORE super keep the undefined — echojs has no TDZ.)
            if (m.this_id) {
                for (let i = 0; i < n.body.body.length; i++) {
                    let s = n.body.body[i];
                    if (
                        s.type === b.ExpressionStatement &&
                        (is_intrinsic(s.expression, "%constructSuper") ||
                            is_intrinsic(s.expression, "%constructSuperApply"))
                    ) {
                        n.body.body.splice(
                            i + 1,
                            0,
                            b.expressionStatement(
                                b.assignmentExpression(
                                    b.identifier(m.this_id),
                                    "=",
                                    b.thisExpression()
                                )
                            )
                        );
                        i++;
                    }
                }
            }
        }
        return n;
    }
}
