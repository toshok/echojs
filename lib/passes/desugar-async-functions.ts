/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */
//
// async functions on the generator coroutines:
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
// async GENERATOR functions ride the same coroutines with a marker
// protocol: the body becomes a sync generator whose awaits yield
// { mark, "await", v } and whose yields yield { mark, "yield", v };
// %asyncGenDrive consumes the markers, serializing next()/throw()/
// return() requests through a queue and settling each with a promised
// iterator result.  `yield*` delegates through a sync relay generator
// (__ejs_agenDelegate) whose marked yields pass through the outer
// generator's `yield*` untouched, so delegated awaits and yields reach
// the driver directly.
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

const AGEN_DRIVE_NAME = "__ejs_asyncGenDrive";
const AGEN_MARK_NAME = "__ejs_agenMark";
const AGEN_DELEGATE_NAME = "__ejs_agenDelegate";

// the async-generator runtime, injected per wrapper like the async
// driver.  Requests (next/throw/return) queue and settle in order; the
// generator resumes synchronously inside the request call (matching
// AsyncGeneratorResumeNext), with awaits and yielded values resolved
// through Promise.resolve.  A yielded value is awaited before delivery;
// its rejection is thrown at the yield.  The relay generator adapts
// `yield*`: async sources have next() results awaited, sync sources get
// each value awaited (async-from-sync), and marked requests it yields
// pass through the delegating generator to the driver unchanged.
const AGEN_RUNTIME_SRC = `
var ${AGEN_MARK_NAME} = {};
function ${AGEN_DRIVE_NAME}(gen) {
    var queue = [];
    var running = false;
    function enqueue(key, arg) {
        return new Promise(function (resolve, reject) {
            queue.push({ key: key, arg: arg, resolve: resolve, reject: reject });
            if (!running) { running = true; pump(); }
        });
    }
    function pump() {
        if (queue.length === 0) { running = false; return; }
        var req = queue[0];
        step(req, req.key, req.arg);
    }
    function finish(req, result) { queue.shift(); req.resolve(result); pump(); }
    function fail(req, e) { queue.shift(); req.reject(e); pump(); }
    function step(req, key, arg) {
        var res;
        try { res = gen[key](arg); }
        catch (e) { fail(req, e); return; }
        var y = res.value;
        if (res.done) {
            Promise.resolve(y).then(
                function (v) { finish(req, { value: v, done: true }); },
                function (e) { fail(req, e); });
            return;
        }
        if (y !== null && typeof y === "object" && y.s === ${AGEN_MARK_NAME}) {
            if (y.k === "await") {
                Promise.resolve(y.v).then(
                    function (v) { step(req, "next", v); },
                    function (e) { step(req, "throw", e); });
            } else {
                Promise.resolve(y.v).then(
                    function (v) { finish(req, { value: v, done: false }); },
                    function (e) { step(req, "throw", e); });
            }
            return;
        }
        finish(req, { value: y, done: false });
    }
    var agen = {};
    agen.next = function (v) { return enqueue("next", v); };
    agen["throw"] = function (e) { return enqueue("throw", e); };
    agen["return"] = function (v) { return enqueue("return", v); };
    agen[Symbol.asyncIterator] = function () { return agen; };
    return agen;
}
function* ${AGEN_DELEGATE_NAME}(z) {
    var useAsync = z[Symbol.asyncIterator] != null;
    var it = useAsync ? z[Symbol.asyncIterator]() : z[Symbol.iterator]();
    var sent = undefined;
    for (;;) {
        var res = it.next(sent);
        if (useAsync) res = yield { s: ${AGEN_MARK_NAME}, k: "await", v: res };
        var v = res.value;
        if (!useAsync) v = yield { s: ${AGEN_MARK_NAME}, k: "await", v: v };
        if (res.done) return v;
        sent = yield { s: ${AGEN_MARK_NAME}, k: "yield", v: v };
    }
}
`;

function agenRuntimeDecls(): e.Statement[] {
    return parse(AGEN_RUNTIME_SRC).body as e.Statement[];
}

function agenDriveCall(genObj: e.Expression): e.Expression {
    return b.callExpression(b.identifier(AGEN_DRIVE_NAME), [genObj]);
}

// rewrites the *direct* body of one async function: awaits become yields,
// `for await` becomes the async-iteration loop.  nested functions are
// their own await scopes and are left alone (the outer pass has already
// processed any async ones among them).  In async-generator mode both
// awaits and yields become marked yields for %asyncGenDrive, and
// `yield*` delegates through the relay generator.
class AwaitToYield extends TreeVisitor {
    constructor(private asyncGen = false) {
        super();
    }

    run<T extends e.Node>(n: T): T {
        return this.visitAs(n);
    }

    private markObj(kind: "await" | "yield", value: e.Expression): e.Expression {
        return b.objectExpression([
            b.property(b.identifier("s"), b.identifier(AGEN_MARK_NAME)),
            b.property(b.identifier("k"), b.literal(kind)),
            b.property(b.identifier("v"), value),
        ]);
    }

    private yieldExpr(arg: e.Expression, delegate = false): e.YieldExpression {
        return { type: "YieldExpression", argument: arg, delegate } as e.YieldExpression;
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
        return this.awaited(this.visitAs(n.argument));
    }

    override visitYield(n: e.YieldExpression): VisitResult {
        // only reachable in async-generator mode: yields in nested plain
        // generators belong to functions this visitor doesn't enter
        const arg = n.argument ? this.visitAs(n.argument) : b.undefinedLit();
        if (n.delegate)
            return this.yieldExpr(b.callExpression(b.identifier(AGEN_DELEGATE_NAME), [arg]), true);
        return this.yieldExpr(this.markObj("yield", arg));
    }

    private awaited(value: e.Expression): e.Expression {
        if (this.asyncGen) return this.yieldExpr(this.markObj("await", value));
        return this.yieldExpr(value);
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
        if (!n.async) return;
        if (n.generator) this.rewriteAsyncGen(n);
        else this.rewriteAsync(n);
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

    // the wrapper's .length must match the original's: as many fresh
    // placeholder params as leading no-default, non-rest formals (the
    // real params live on the inner generator; calls forward arguments)
    private lengthParams(n: e.Function): e.Pattern[] {
        let count = 0;
        for (let i = 0; i < n.params.length; i++) {
            if (n.params[i]!.type === "RestElement" || n.defaults[i] != null) break;
            count++;
        }
        return Array.from({ length: count }, () => fresh("arg"));
    }

    // async function* f(a) { ... }  becomes
    //
    //   function f() {
    //       <agen runtime: mark + %asyncGenDrive + relay>
    //       return __ejs_asyncGenDrive(function* (a) { <marked body> }
    //                                  .apply(this, arguments));
    //   }
    //
    // (arrows can't be generators, so only the function forms arrive)
    private rewriteAsyncGen(n: e.Function): void {
        const genBody = new AwaitToYield(true).run(n.body as e.BlockStatement);
        const genFn = b.functionExpression(null, n.params, genBody, n.defaults);
        genFn.generator = true;
        n.params = this.lengthParams(n);
        n.defaults = [];
        n.async = false;
        n.generator = false;
        n.body = b.blockStatement([
            ...agenRuntimeDecls(),
            b.returnStatement(
                agenDriveCall(
                    b.callExpression(b.memberExpression(genFn, b.identifier("apply")), [
                        b.thisExpression(),
                        b.identifier("arguments"),
                    ])
                )
            ),
        ]);
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
        n.params = this.lengthParams(n);
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
