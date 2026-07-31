/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */
//
// desugars the post-ES6 operator forms into plain ES6:
//
//   optional chains:
//     a?.b.c        =>  (() => { let %t = a; if (%t == null) return; return %t.b.c; })()
//     a.b?.(x)      =>  (() => { let %t = a; let %f = %t.b; if (%f == null) return;
//                                return %f.call(%t, x); })()
//     delete a?.b   =>  (() => { let %t = a; if (%t == null) return true; return delete %t.b; })()
//
//   logical assignment:
//     x &&= v       =>  x && (x = v)
//     o.p ??= v     =>  (() => { let %t = o; return %t.p ?? (%t.p = v); })()
//
// the arrow bodies keep `this`/`arguments`/`super` bindings lexical, so
// the wrapping is transparent; DesugarClasses (which runs later) still
// sees and rewrites any `super` references inside.
//
// runs FIRST among the pre-EIR passes: everything it emits is plain ES6
// (plus `??`, which EIR lowers natively) for the later passes to consume.

import { TransformPass, VisitResult } from "../node-visitor";
import * as b from "../ast-builder";
import { startGenerator } from "../echo-util";
import type * as e from "../estree";

const gen = startGenerator();
const fresh = () => b.identifier(`%opt_tmp${gen()}`);

const logical_assign_ops: Record<string, "&&" | "||" | "??" | undefined> = {
    "&&=": "&&",
    "||=": "||",
    "??=": "??",
};

// `%t == null` — the nullish test (no valueOf involvement, per spec)
function isNullish(id: e.Identifier): e.Expression {
    return b.binaryExpression(b.identifier(id.name), "==", b.nullLit());
}

// wrap statements in an immediately-invoked arrow (lexical this/arguments)
function iife(stmts: e.Statement[]): e.Expression {
    return b.callExpression(b.arrowFunctionExpression([], b.blockStatement(stmts)), []);
}

export class DesugarModernOps extends TransformPass {
    // ---- optional chaining ------------------------------------------------

    override visitChainExpression(n: e.ChainExpression): VisitResult {
        return this.desugarChain(n.expression, null);
    }

    override visitUnaryExpression(n: e.UnaryExpression): VisitResult {
        // `delete a?.b` short-circuits to true
        if (n.operator === "delete" && n.argument.type === "ChainExpression")
            return this.desugarChain(n.argument.expression, "delete");
        return super.visitUnaryExpression(n);
    }

    private desugarChain(expr: e.MemberExpression | e.CallExpression, mode: "delete" | null): e.Expression {
        const stmts: e.Statement[] = [];
        const shortCircuit = mode === "delete" ? b.literal(true) : null;
        const result = this.chainLink(expr, stmts, shortCircuit).value;
        stmts.push(
            b.returnStatement(
                mode === "delete" ? b.unaryExpression("delete", result) : result
            )
        );
        return iife(stmts);
    }

    // lower one chain link, appending its prefix statements.  the returned
    // value expression is not yet evaluated — the caller embeds it exactly
    // once.  thisRef carries the receiver temp for `a.b?.()`-style calls.
    private chainLink(
        node: e.Expression | e.Super,
        stmts: e.Statement[],
        shortCircuit: e.Expression | null
    ): { value: e.Expression; thisRef?: e.Identifier | "this" } {
        if (node.type === "MemberExpression") {
            const prop = node.computed ? this.visitAs(node.property) : node.property;
            if (node.object.type === "Super") {
                // `super` is not a value: keep the member intact (only the
                // link after it can be optional; `super?.x` is a parse
                // error).  a call through it receives `this`.
                const member = b.memberExpression(node.object, prop as e.Expression, node.computed);
                return { value: member, thisRef: "this" };
            }
            const obj = this.chainLink(node.object, stmts, shortCircuit).value;
            const t = fresh();
            stmts.push(b.letDeclaration(t, obj));
            if (node.optional)
                stmts.push(b.ifStatement(isNullish(t), b.returnStatement(shortCircuit)));
            const member = b.memberExpression(b.identifier(t.name), prop as e.Expression);
            member.computed = node.computed;
            return { value: member, thisRef: t };
        }

        if (node.type === "CallExpression") {
            const callee = this.chainLink(node.callee, stmts, shortCircuit);
            const args = this.visitArray(node.arguments);
            if (!node.optional)
                return { value: b.callExpression(callee.value, args) };
            const f = fresh();
            stmts.push(b.letDeclaration(f, callee.value));
            stmts.push(b.ifStatement(isNullish(f), b.returnStatement(shortCircuit)));
            if (callee.thisRef) {
                // preserve the receiver:  %f.call(%t, args)
                const receiver =
                    callee.thisRef === "this"
                        ? b.thisExpression()
                        : b.identifier(callee.thisRef.name);
                return {
                    value: b.callExpression(
                        b.memberExpression(b.identifier(f.name), b.identifier("call")),
                        [receiver, ...args]
                    ),
                };
            }
            return { value: b.callExpression(b.identifier(f.name), args) };
        }

        // the chain base: any other expression (nested chains arrive as
        // their own ChainExpression and desugar through the normal visit)
        return { value: this.visitAs(node as e.Expression) };
    }

    // ---- logical assignment ----------------------------------------------

    override visitAssignmentExpression(n: e.AssignmentExpression): VisitResult {
        const logical_op = logical_assign_ops[n.operator as string];
        if (!logical_op) return super.visitAssignmentExpression(n);

        const right = this.visitAs(n.right);

        if (n.left.type === "Identifier") {
            // x op= v  =>  x op (x = v)  (fresh nodes per value-position use)
            return b.logicalExpression(
                b.identifier(n.left.name),
                logical_op,
                b.assignmentExpression(b.identifier(n.left.name), "=", right)
            );
        }

        if (n.left.type !== "MemberExpression")
            throw new Error(
                `unexpected logical-assignment target ${n.left.type} in DesugarModernOps`
            );

        // o.p op= v  =>  (() => { let %t = o; return %t.p op (%t.p = v); })()
        const target = n.left;
        const stmts: e.Statement[] = [];
        const t = fresh();
        stmts.push(b.letDeclaration(t, this.visitAs(target.object as e.Expression)));

        let mkProp: () => e.Expression;
        if (target.computed) {
            const k = fresh();
            stmts.push(b.letDeclaration(k, this.visitAs(target.property as e.Expression)));
            mkProp = () => b.identifier(k.name);
        } else if ((target.property as e.Node).type === "PrivateIdentifier") {
            // o.#x op= v: keep the private member; DesugarClasses rewrites
            // the read and the write when it runs later
            const name = (target.property as e.PrivateIdentifier).name;
            mkProp = () => ({ type: "PrivateIdentifier", name }) as unknown as e.Expression;
        } else {
            const name = (target.property as e.Identifier).name;
            mkProp = () => b.identifier(name);
        }
        const mkMember = () => {
            const m = b.memberExpression(b.identifier(t.name), mkProp());
            m.computed = target.computed;
            return m;
        };

        stmts.push(
            b.returnStatement(
                b.logicalExpression(
                    mkMember(),
                    logical_op,
                    b.assignmentExpression(mkMember(), "=", right)
                )
            )
        );
        return iife(stmts);
    }
}
