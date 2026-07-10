/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

//
// desugars
//
//   [1, 2, ...foo, 3, 4]
//
//   o.foo(1, 2, ...foo, 3, 4)
//
// to:
//
//   %arrayFromSpread([1, 2], foo, [3, 4])
//
//   o.foo.apply(o, %arrayFromSpread([1, 2], foo, [3, 4])
//

import { TransformPass } from "../node-visitor";
import * as b from "../ast-builder";
import { intrinsic, is_intrinsic } from "../echo-util";
import { arrayFromSpread_id, apply_id, constructSuperApply_id } from "../common-ids";

// split `args` into %arrayFromSpread operands: runs of plain arguments
// become array literals, spread arguments pass through as iterables
function spreadChunks(args) {
    let chunks = [];
    let current = [];
    for (let el of args) {
        if (el.type === b.SpreadElement) {
            if (current.length > 0) {
                chunks.push(b.arrayExpression(current));
                current = [];
            }
            chunks.push(el.argument);
        } else {
            current.push(el);
        }
    }
    if (current.length > 0) chunks.push(b.arrayExpression(current));
    return chunks;
}

export class DesugarSpread extends TransformPass {
    visitArrayExpression(n) {
        n = super.visitArrayExpression(n);
        let needs_desugaring = false;
        for (let el of n.elements) {
            if (el && el.type === b.SpreadElement) {
                needs_desugaring = true;
                break;
            }
        }

        if (!needs_desugaring) return n;

        let new_args = [];
        let current_elements = [];
        for (let el of n.elements) {
            if (el && el.type === b.SpreadElement) {
                if (current_elements.length === 0) {
                    // just push the spread argument into the new args
                    new_args.push(el.argument);
                } else {
                    // push the current_elements as an array literal, then the spread.
                    // also reset current_elements to []
                    new_args.push(b.arrayExpression(current_elements));
                    new_args.push(el.argument);
                    current_elements = [];
                }
            } else {
                current_elements.push(el);
            }
        }
        if (current_elements.length > 0) new_args.push(b.arrayExpression(current_elements));

        // check to see if we've just created an array of nothing but array literals, and flatten them all
        // into one and get rid of the spread altogether
        let all_arrays = true;
        for (let a of new_args) {
            if (a.type !== b.ArrayExpression) all_arrays = false;
        }

        if (all_arrays) {
            let na = [];
            for (let a of new_args) na = na.concat(a.elements);
            n.elements = na;
            return n;
        } else {
            return intrinsic(arrayFromSpread_id, new_args);
        }
    }

    visitCallExpression(n) {
        n = super.visitCallExpression(n);

        // super(...args) / super.foo(...args) can't be rewritten to an
        // .apply call.  this pass now runs before DesugarClasses (pre-EIR);
        // leave super calls alone — DesugarClasses rewrites them into
        // ordinary calls, and the post-classes run of this pass desugars
        // whatever spreads remain.
        if (n.callee.type === b.Super) return n;
        if (n.callee.type === b.MemberExpression && n.callee.object.type === b.Super) return n;

        let needs_desugaring = false;
        for (let el of n.arguments) {
            if (el.type === b.SpreadElement) {
                needs_desugaring = true;
                break;
            }
        }

        if (!needs_desugaring) return n;

        // super(...args), already desugared by DesugarClasses (which runs
        // first) into %constructSuper(ref, ...args): the intrinsic isn't a
        // value and can't be .apply'd — use the runtime's apply form.
        // (spread super calls didn't compile at all before this.)
        if (is_intrinsic(n, "%constructSuper")) {
            let super_ref = n.arguments[0];
            let chunks = spreadChunks(n.arguments.slice(1));
            if (chunks.every((a) => a.type === b.ArrayExpression)) {
                // spreads of array literals only: flatten back to a plain
                // %constructSuper (holes become undefined, as below)
                let flat = [];
                for (let a of chunks)
                    flat = flat.concat(a.elements.map((el) => (el === null ? b.undefinedLit() : el)));
                n.arguments = [super_ref].concat(flat);
            } else {
                n.callee = constructSuperApply_id;
                n.arguments = [super_ref, intrinsic(arrayFromSpread_id, chunks)];
            }
            return n;
        }

        let new_args = [];
        let current_elements = [];
        for (let el of n.arguments) {
            if (el.type === b.SpreadElement) {
                if (current_elements.length === 0) {
                    // just push the spread argument into the new args
                    new_args.push(el.argument);
                } else {
                    // push the current_elements as an array literal, then the spread.
                    // also reset current_elements to []
                    new_args.push(b.arrayExpression(current_elements));
                    new_args.push(el.argument);
                    current_elements = [];
                }
            } else {
                current_elements.push(el);
            }
        }

        if (current_elements.length > 0) new_args.push(b.arrayExpression(current_elements));

        // check to see if we've just created an array of nothing but array literals, and flatten them all
        // into one and get rid of the spread altogether
        let all_arrays = true;
        for (let a of new_args) {
            if (a.type !== b.ArrayExpression) {
                all_arrays = false;
                break;
            }
        }
        if (all_arrays) {
            let na = [];
            for (let a of new_args) na = na.concat(a.elements);

            // if we're converting an array with holes into arguments for a function, hole => undefined
            na = na.map((el) => (el === null ? b.undefinedLit() : el));

            n.arguments = na;
        } else {
            let receiver;

            if (n.callee.type === b.MemberExpression) receiver = n.callee.object;
            else receiver = b.nullLit();

            n.callee = b.memberExpression(n.callee, apply_id);
            n.arguments = [receiver, intrinsic(arrayFromSpread_id, new_args)];
        }

        return n;
    }
}
