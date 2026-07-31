/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
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

import { TransformPass, VisitResult } from "../node-visitor";
import * as b from "../ast-builder";
import { intrinsic, is_intrinsic } from "../echo-util";
import {
    arrayFromSpread_id,
    apply_id,
    constructSuperApply_id,
    constructApply_id,
    copyDataProps_id,
    objectSpreadMerge_id,
} from "../common-ids";
import type * as e from "../estree";

// split `args` into %arrayFromSpread operands: runs of plain arguments
// become array literals, spread arguments pass through as iterables
function spreadChunks(args: (e.Expression | e.SpreadElement | null)[]): e.Expression[] {
    const chunks: e.Expression[] = [];
    let current: (e.Expression | e.SpreadElement | null)[] = [];
    for (const el of args) {
        if (el && el.type === "SpreadElement") {
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

// holes become undefined when array elements turn into call arguments
function holeToUndefined(el: e.Expression | e.SpreadElement | null): e.Expression | e.SpreadElement {
    return el === null ? b.undefinedLit() : el;
}

export class DesugarSpread extends TransformPass {
    // { a: 1, ...x, get b() {} }  =>
    //   %objectSpreadMerge(%copyDataProps({ a: 1 }, x, undefined), { get b() {} })
    // consecutive plain properties stay one literal chunk (the emitter's
    // native path); spread sources copy per CopyDataProperties; post-spread
    // literal chunks merge by descriptor so accessors survive
    override visitObjectExpression(n: e.ObjectExpression): VisitResult {
        super.visitObjectExpression(n);
        if (!n.properties.some((p) => p.type === "SpreadElement")) return n;

        const chunks: (e.ObjectExpression | { spread: e.Expression })[] = [];
        let current: e.Property[] = [];
        for (const prop of n.properties) {
            if (prop.type === "SpreadElement") {
                if (current.length > 0) {
                    chunks.push(b.objectExpression(current));
                    current = [];
                }
                chunks.push({ spread: prop.argument });
            } else {
                current.push(prop);
            }
        }
        if (current.length > 0) chunks.push(b.objectExpression(current));

        let acc: e.Expression =
            chunks[0] && !("spread" in chunks[0])
                ? (chunks.shift() as e.ObjectExpression)
                : b.objectExpression([]);
        for (const chunk of chunks) {
            if ("spread" in chunk)
                acc = intrinsic(copyDataProps_id, [acc, chunk.spread, b.undefinedLit()]);
            else acc = intrinsic(objectSpreadMerge_id, [acc, chunk]);
        }
        return acc;
    }

    override visitArrayExpression(n: e.ArrayExpression): VisitResult {
        super.visitArrayExpression(n);
        const needs_desugaring = n.elements.some((el) => el && el.type === "SpreadElement");
        if (!needs_desugaring) return n;

        const chunks = spreadChunks(n.elements);
        if (chunks.every((a) => a.type === "ArrayExpression")) {
            // spreads of array literals only: flatten back into one literal
            let flat: (e.Expression | e.SpreadElement | null)[] = [];
            for (const a of chunks) flat = flat.concat((a as e.ArrayExpression).elements);
            n.elements = flat;
            return n;
        }
        return intrinsic(arrayFromSpread_id, chunks);
    }

    // new Foo(...args) -> %constructApply(Foo, %arrayFromSpread(...));
    // constructs through the runtime's dense-array apply
    override visitNewExpression(n: e.NewExpression): VisitResult {
        super.visitNewExpression(n);
        if (!n.arguments.some((el) => el.type === "SpreadElement")) return n;
        const chunks = spreadChunks(n.arguments);
        if (chunks.every((a) => a.type === "ArrayExpression")) {
            let flat: (e.Expression | e.SpreadElement)[] = [];
            for (const a of chunks)
                flat = flat.concat((a as e.ArrayExpression).elements.map(holeToUndefined));
            n.arguments = flat;
            return n;
        }
        return intrinsic(constructApply_id, [n.callee, intrinsic(arrayFromSpread_id, chunks)]);
    }

    override visitCallExpression(n: e.CallExpression): VisitResult {
        super.visitCallExpression(n);

        // super(...args) / super.foo(...args) can't be rewritten to an
        // .apply call.  this pass runs before DesugarClasses; leave super
        // calls alone — DesugarClasses rewrites them into ordinary calls,
        // and the post-classes run of this pass desugars what remains.
        if (n.callee.type === "Super") return n;
        if (n.callee.type === "MemberExpression" && n.callee.object.type === "Super") return n;

        const needs_desugaring = n.arguments.some((el) => el.type === "SpreadElement");
        if (!needs_desugaring) return n;

        // super(...args), already desugared by DesugarClasses (which runs
        // first) into %constructSuper(ref, ...args): the intrinsic isn't a
        // value and can't be .apply'd — use the runtime's apply form.
        if (is_intrinsic(n, "%constructSuper")) {
            const super_ref = n.arguments[0] as e.Expression;
            const chunks = spreadChunks(n.arguments.slice(1));
            if (chunks.every((a) => a.type === "ArrayExpression")) {
                // spreads of array literals only: flatten back to a plain
                // %constructSuper (holes become undefined, as below)
                let flat: (e.Expression | e.SpreadElement)[] = [];
                for (const a of chunks)
                    flat = flat.concat((a as e.ArrayExpression).elements.map(holeToUndefined));
                n.arguments = [super_ref, ...flat];
            } else {
                n.callee = constructSuperApply_id;
                n.arguments = [super_ref, intrinsic(arrayFromSpread_id, chunks)];
            }
            return n;
        }

        const chunks = spreadChunks(n.arguments);
        if (chunks.every((a) => a.type === "ArrayExpression")) {
            // if we're converting an array with holes into arguments for a
            // function, hole => undefined
            let flat: (e.Expression | e.SpreadElement)[] = [];
            for (const a of chunks)
                flat = flat.concat((a as e.ArrayExpression).elements.map(holeToUndefined));
            n.arguments = flat;
            return n;
        }

        const receiver: e.Expression =
            n.callee.type === "MemberExpression" ? (n.callee.object as e.Expression) : b.nullLit();

        n.callee = b.memberExpression(n.callee, apply_id);
        n.arguments = [receiver, intrinsic(arrayFromSpread_id, chunks)];
        return n;
    }
}
