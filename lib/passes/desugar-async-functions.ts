/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */
//
// async functions on the generator coroutines (language-P3):
//
//   async function f(a) { let v = await p; return v + a; }
//
// becomes
//
//   function f() {
//       return %asyncDrive(function* (a) { let v = yield p; return v + a; }
//                          .apply(this, arguments));
//   }
//
// %asyncDrive (injected once per module that needs it) steps the
// generator, resolving each yielded value through Promise.resolve and
// resuming with next()/throw(); the function's result is a Promise.
// Arrows keep their params and close over them (`.call(this)` keeps the
// lexical receiver; arrows have no `arguments` of their own to forward).
//
// `for await (x of src)` (inside an async function) desugars into the
// async-iteration protocol: prefer src[Symbol.asyncIterator](), fall back
// to the sync iterator with each value awaited (async-from-sync).
//
// async GENERATOR functions (async function*) have no lowering and are
// gated at the parser seam.
//
// runs before DesugarClasses (async methods' values are plain functions
// by the time the class machinery sees them) and before
// DesugarDestructuring/DesugarGeneratorFunctions (moved params and the
// synthesized generators take the normal pipeline).

import { TreeVisitor, TransformPass, VisitResult } from "../node-visitor";
import * as b from "../ast-builder";
import { startGenerator } from "../echo-util";
import { reportError } from "../errors";
import { parse } from "../parser";
import type * as e from "../estree";

const gen = startGenerator();
const fresh = (tag: string) => b.identifier(`%async_${tag}${gen()}`);

// %-names are reserved for intrinsics in EIR scope analysis; the driver
// is an ordinary function-scoped binding
const DRIVE_NAME = "__ejs_asyncDrive";

// the driver, in plain ES6 — parsed fresh per injection (AST nodes must
// not be shared between modules: downstream maps are node-keyed)
const DRIVER_SRC = `
function __ejs_asyncDrive(gen) {
    return new Promise(function (resolve, reject) {
        function step(key, arg) {
            var res;
            try { res = gen[key](arg); }
            catch (e) { reject(e); return; }
            if (res.done) { resolve(res.value); return; }
            Promise.resolve(res.value).then(
                function (v) { step("next", v); },
                function (e) { step("throw", e); }
            );
        }
        step("next", undefined);
    });
}
`;

function driverDecl(): e.FunctionDeclaration {
    const program = parse(DRIVER_SRC);
    const decl = program.body[0] as e.FunctionDeclaration;
    decl.id.name = DRIVE_NAME;
    return decl;
}

function driveCall(genObj: e.Expression): e.Expression {
    return b.callExpression(b.identifier(DRIVE_NAME), [genObj]);
}

// rewrites the *direct* body of one async function: awaits become yields,
// `for await` becomes the async-iteration loop.  nested functions are
// their own await scopes and are left alone (the outer pass has already
// processed any async ones among them).
class AwaitToYield extends TreeVisitor {
    run<T extends e.Node>(n: T): T {
        return this.visitAs(n);
    }

    override visitFunctionDeclaration(n: e.FunctionDeclaration): VisitResult {
        return n;
    }
    override visitFunctionExpression(n: e.FunctionExpression): VisitResult {
        return n;
    }
    override visitArrowFunctionExpression(n: e.ArrowFunctionExpression): VisitResult {
        return n;
    }

    override visitAwaitExpression(n: e.AwaitExpression): VisitResult {
        const arg = this.visitAs(n.argument);
        return { type: "YieldExpression", argument: arg, delegate: false } as e.YieldExpression;
    }

    private awaited(value: e.Expression): e.Expression {
        return { type: "YieldExpression", argument: value, delegate: false } as e.YieldExpression;
    }

    override visitForOf(n: e.ForOfStatement): VisitResult {
        if (!n.await) return super.visitForOf(n);

        const right = this.visitAs(n.right);
        const body = this.visitAs(n.body);

        const src = fresh("src");
        const useAsync = fresh("useasync");
        const iter = fresh("iter");
        const res = fresh("res");
        const val = fresh("val");

        const symbolMethod = (name: string) =>
            b.memberExpression(
                b.identifier(src.name),
                b.memberExpression(b.identifier("Symbol"), b.identifier(name)),
                true
            );

        // let %src = right;
        // let %useAsync = %src[Symbol.asyncIterator] != null;
        // let %iter = %useAsync ? %src[Symbol.asyncIterator]() : %src[Symbol.iterator]();
        const setup: e.Statement[] = [
            b.letDeclaration(src, right),
            b.letDeclaration(
                useAsync,
                b.binaryExpression(symbolMethod("asyncIterator"), "!=", b.nullLit())
            ),
            b.letDeclaration(
                iter,
                b.conditionalExpression(
                    b.identifier(useAsync.name),
                    b.callExpression(symbolMethod("asyncIterator"), []),
                    b.callExpression(symbolMethod("iterator"), [])
                )
            ),
        ];

        // the per-iteration binding: reuse the original head
        let bindStmt: e.Statement;
        if (n.left.type === "VariableDeclaration") {
            const d = n.left.declarations[0]!;
            bindStmt = b.variableDeclaration(n.left.kind, [
                b.variableDeclarator(d.id, b.identifier(val.name)),
            ]);
        } else {
            bindStmt = b.expressionStatement(
                b.assignmentExpression(n.left, "=", b.identifier(val.name))
            );
        }

        const loopBody: e.Statement[] = [
            // let %res = await %iter.next();
            b.letDeclaration(
                res,
                this.awaited(
                    b.callExpression(
                        b.memberExpression(b.identifier(iter.name), b.identifier("next")),
                        []
                    )
                )
            ),
            b.ifStatement(
                b.memberExpression(b.identifier(res.name), b.identifier("done")),
                b.breakStatement()
            ),
            b.letDeclaration(
                val,
                b.memberExpression(b.identifier(res.name), b.identifier("value"))
            ),
            // async-from-sync: each value is awaited too
            b.ifStatement(
                b.unaryExpression("!", b.identifier(useAsync.name)),
                b.expressionStatement(
                    b.assignmentExpression(
                        b.identifier(val.name),
                        "=",
                        this.awaited(b.identifier(val.name))
                    )
                )
            ),
            bindStmt,
            body,
        ];

        return b.blockStatement([
            ...setup,
            b.whileStatement(b.literal(true), b.blockStatement(loopBody)),
        ]);
    }
}

export class DesugarAsyncFunctions extends TransformPass {
    // innermost enclosing function's asyncness during the descent (the
    // rewrite itself happens on the way back up)
    private fnAsyncStack: boolean[] = [];

    private visitFn(n: e.Function, visitSuper: () => void): void {
        this.fnAsyncStack.push(n.async === true);
        visitSuper();
        this.fnAsyncStack.pop();
        if (n.async) this.rewriteAsync(n);
    }

    override visitFunctionDeclaration(n: e.FunctionDeclaration): VisitResult {
        this.visitFn(n, () => super.visitFunctionDeclaration(n));
        return n;
    }

    override visitFunctionExpression(n: e.FunctionExpression): VisitResult {
        this.visitFn(n, () => super.visitFunctionExpression(n));
        return n;
    }

    override visitArrowFunctionExpression(n: e.ArrowFunctionExpression): VisitResult {
        this.visitFn(n, () => super.visitArrowFunctionExpression(n));
        return n;
    }

    override visitAwaitExpression(n: e.AwaitExpression): VisitResult {
        // legitimate awaits ride along here and convert when the enclosing
        // async function is rewritten on the way back up; an await outside
        // any async function has no lowering (top-level await included)
        if (this.fnAsyncStack[this.fnAsyncStack.length - 1]) {
            n.argument = this.visitAs(n.argument);
            return n;
        }
        reportError(
            SyntaxError,
            "await is only valid in async functions",
            this.filename,
            n.loc ?? undefined
        );
    }

    private rewriteAsync(n: e.Function): void {
        const a2y = new AwaitToYield();
        let genBody: e.BlockStatement;
        if (n.body.type === "BlockStatement") {
            genBody = a2y.run(n.body);
        } else {
            genBody = b.blockStatement([b.returnStatement(a2y.run(n.body as e.Expression))]);
        }

        // the driver rides inside each wrapper (a toplevel injection would
        // need module-slot registration — gather-imports has already run)
        if (n.type === "ArrowFunctionExpression") {
            // params stay on the arrow; the generator closes over them and
            // runs with the arrow's (lexical) this
            const genFn = b.functionExpression(null, [], genBody, []);
            genFn.generator = true;
            n.async = false;
            n.body = b.blockStatement([
                driverDecl(),
                b.returnStatement(
                    driveCall(
                        b.callExpression(b.memberExpression(genFn, b.identifier("call")), [
                            b.thisExpression(),
                        ])
                    )
                ),
            ]);
            n.expression = false;
            return;
        }

        // function form: params (incl. rest/patterns/defaults) move to the
        // generator; this and arguments forward through .apply
        const genFn = b.functionExpression(null, n.params, genBody, n.defaults);
        genFn.generator = true;
        n.params = [];
        n.defaults = [];
        n.async = false;
        n.body = b.blockStatement([
            driverDecl(),
            b.returnStatement(
                driveCall(
                    b.callExpression(b.memberExpression(genFn, b.identifier("apply")), [
                        b.thisExpression(),
                        b.identifier("arguments"),
                    ])
                )
            ),
        ]);
    }
}
