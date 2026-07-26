/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// EIR unit tests.  run (via the tsjs+babel tree) with:
//   node lib/generated/lib/eir/tests.js
// or through buck:
//   buck2 build //:test-eir

import { FunctionBuilder } from "./builder";
import { printFunction, printModule } from "./printer";
import { verifyFunction, verifyModule } from "./verifier";
import { lowerFunctionNode, lowerProgram, lowerAnalyzedFunction } from "./lower";
import { optimizeFunction, optimizeModule } from "./optimize";
import type { OptStats } from "./optimize";
import { specializeModule } from "./specialize";
import { ScopeAnalysis } from "./scopes";
import { isLowerNotSupported } from "./errors";
import { Func, Block, Inst, Module } from "./ir";
import { DesugarSpread } from "../passes/desugar-spread";
import { typeSigToEirType, typeSigToShapeRepr } from "./oracle";
import type { OracleShapeField, TypeOracle, TypeTag } from "./oracle";
import { optimizeShapeRegions } from "./optimize-guards";
import { sinkConstructResults } from "./sink-construct";
import { buildArithDiamond, buildLowTierAdd, buildLowTierLt } from "./lowtier-probe";
import { DesugarClasses } from "../passes/desugar-classes";
import { DesugarDestructuring } from "../passes/desugar-destructuring";
import { DesugarGeneratorFunctions } from "../passes/desugar-generator-functions";
import { DesugarMetaProperties } from "../passes/desugar-metaproperties";
import * as esprima from "../../external-deps/esprima/esprima-es6";
import type * as e from "../estree";
import type { CompilerOptions } from "../options";

let failures = 0;

function test(name: string, fn: () => void): void {
    try {
        fn();
        console.log(`pass: ${name}`);
    } catch (err) {
        failures++;
        const failure = err as Error;
        console.log(`FAIL: ${name}: ${failure.message}`);
        if (failure.stack) console.log(failure.stack.split("\n").slice(1, 4).join("\n"));
    }
}

function assert(cond: boolean, msg?: string): void {
    if (!cond) throw new Error(`assertion failed: ${msg || ""}`);
}

function assertContains(haystack: string, needle: string): void {
    if (haystack.indexOf(needle) === -1)
        throw new Error(`expected output to contain '${needle}'\n---\n${haystack}\n---`);
}

function findBlock(fn: Func, prefix: string): Block {
    for (let b of fn.blocks) if (b.name.indexOf(prefix) === 0) return b;
    throw new Error(`no block named ${prefix}* in @${fn.name}`);
}

function findFn(mod: Module, name: string): Func {
    for (let f of mod.functions) if (f.name === name) return f;
    throw new Error(`no function @${name} in module`);
}

function parseFn(src: string): e.FunctionDeclaration {
    let ast = esprima.parse(src, { loc: true, raw: true });
    for (let s of ast.body) if (s.type === "FunctionDeclaration") return s;
    throw new Error("no function declaration in source");
}

function lowerOne(src: string): { module: Module; fn: Func } {
    let r = lowerFunctionNode(parseFn(src));
    verifyModule(r.module);
    return r;
}

// --- builder ------------------------------------------------------------------

test("builder: diamond join inserts a block param", () => {
    let fb = new FunctionBuilder("diamond", ["c"]);

    let then_bb = fb.newBlock("then");
    let else_bb = fb.newBlock("else");
    let join_bb = fb.newBlock("join");

    let cond = fb.readVariable("c", fb.cur);
    fb.condBr(cond, then_bb, [], else_bb, []);
    fb.sealBlock(then_bb);
    fb.sealBlock(else_bb);

    fb.setInsertPoint(then_bb);
    fb.writeVariable("x", then_bb, fb.constNumber(1));
    fb.br(join_bb, []);

    fb.setInsertPoint(else_bb);
    fb.writeVariable("x", else_bb, fb.constNumber(2));
    fb.br(join_bb, []);
    fb.sealBlock(join_bb);

    fb.setInsertPoint(join_bb);
    let x = fb.readVariable("x", join_bb);
    fb.ret(x);

    let fn = fb.finish();
    verifyFunction(fn);

    assert(x.op === "blockparam", "join read should be a block param");
    assert(findBlock(fn, "join").params.length === 1, "join should have exactly one param");
});

test("builder: same value in both arms leaves no param", () => {
    let fb = new FunctionBuilder("nodifference", ["c"]);

    let v = fb.constNumber(42);
    fb.writeVariable("x", fb.cur, v);

    let then_bb = fb.newBlock("then");
    let else_bb = fb.newBlock("else");
    let join_bb = fb.newBlock("join");

    fb.condBr(fb.readVariable("c", fb.cur), then_bb, [], else_bb, []);
    fb.sealBlock(then_bb);
    fb.sealBlock(else_bb);

    fb.setInsertPoint(then_bb);
    fb.br(join_bb, []);
    fb.setInsertPoint(else_bb);
    fb.br(join_bb, []);
    fb.sealBlock(join_bb);

    fb.setInsertPoint(join_bb);
    let x = fb.readVariable("x", join_bb);
    fb.ret(x);

    let fn = fb.finish();
    verifyFunction(fn);
    assert(x === v, "read through the join should see the original value");
    assert(findBlock(fn, "join").params.length === 0, "no params expected at join");
});

// --- lowering: SSA shapes -------------------------------------------------------

test("lower: loop-invariant variable gets no header param", () => {
    let { fn } = lowerOne("function f(c) { let a = 5; while (c) { } return a; }");
    assert(findBlock(fn, "while_header").params.length === 0, "header should have no params");
});

test("lower: loop counter gets exactly one header param", () => {
    let { fn } = lowerOne(
        "function g(n) { let i = 0; while (i < n) { i = i + 1; } return i; }"
    );
    let header = findBlock(fn, "while_header");
    assert(header.params.length === 1, `header params = ${header.params.length}`);
});

test("lower: if/else assigns and joins", () => {
    let { fn } = lowerOne(
        "function h(c) { let x = 0; if (c) { x = 1; } else { x = 2; } return x; }"
    );
    assert(findBlock(fn, "if_join").params.length === 1, "join should have one param");
});

test("lower: logical && short-circuits through a join param", () => {
    let { fn } = lowerOne("function a(x, y) { return x && y; }");
    let printed = printFunction(fn);
    assertContains(printed, "logical_join");
    assertContains(printed, "cond_br");
});

test("lower: method calls and property access", () => {
    let { fn } = lowerOne(
        "function m(o) { o.count = o.count + 1; return o.get(o.count, 3); }"
    );
    let printed = printFunction(fn);
    assertContains(printed, 'get_prop_atom');
    assertContains(printed, 'atom="count"');
    assertContains(printed, 'set_prop_atom');
    assertContains(printed, "call");
});

test("lower: globals resolve to get_global", () => {
    let { fn } = lowerOne("function p(x) { return console.log(x); }");
    assertContains(printFunction(fn), 'get_global atom="console"');
});

test("lower: for loop with break/continue", () => {
    let { fn } = lowerOne(
        "function bc(n) { let s = 0; " +
            "for (let i = 0; i < n; i = i + 1) { " +
            "if (i === 3) continue; if (i === 7) break; s = s + i; } " +
            "return s; }"
    );
    let header = findBlock(fn, "for_header");
    assert(header.params.length === 2, `header params = ${header.params.length} (want s, i)`);
});

test("lower: do-while", () => {
    let { fn } = lowerOne(
        "function dw(n) { let i = 0; do { i = i + 1; } while (i < n); return i; }"
    );
    findBlock(fn, "do_body");
    findBlock(fn, "do_cond");
});

// --- lowering: closures / environments --------------------------------------------

test("lower: closure counter allocates an env and captures", () => {
    let { module, fn } = lowerOne(
        "function outer() { let c = 0; function inc() { c = c + 1; return c; } return inc; }"
    );
    assert(module.functions.length === 2, "module should have outer + inc");

    let printed_outer = printFunction(fn);
    assertContains(printed_outer, "make_env size=1");
    assertContains(printed_outer, "env_store");
    assertContains(printed_outer, 'make_closure');
    assertContains(printed_outer, 'fn="outer.inc"');

    let inc = findFn(module, "outer.inc");
    let printed_inc = printFunction(inc);
    assertContains(printed_inc, "env_load");
    assertContains(printed_inc, "env_store");
});

test("lower: capture through an env-less intermediate function", () => {
    let { module } = lowerOne(
        "function o() { let x = 1; " +
            "function mid() { function inner() { return x; } return inner; } " +
            "return mid; }"
    );
    assert(module.functions.length === 3, "module should have o, mid, inner");

    // mid captures nothing itself: no env of its own, it forwards its
    // incoming env to inner's closure
    let mid = findFn(module, "o.mid");
    let printed_mid = printFunction(mid);
    assert(printed_mid.indexOf("make_env") === -1, "mid should not allocate an env");
    assertContains(printed_mid, "make_closure");

    // inner reads x straight out of its incoming env (zero hops)
    let inner = findFn(module, "o.mid.inner");
    let printed_inner = printFunction(inner);
    assertContains(printed_inner, "env_load");
    assert(printed_inner.indexOf("slot=0") !== -1, "x should live in slot 0 of o's env");
});

test("lower: captured parameter is stored to the env at entry", () => {
    let { fn } = lowerOne(
        "function k(x) { function get() { return x; } return get; }"
    );
    let printed = printFunction(fn);
    assertContains(printed, "make_env size=1");
    assertContains(printed, "env_store");
});

test("lower: function expressions become closures", () => {
    let { module, fn } = lowerOne(
        "function fe() { let f = function (a) { return a + 1; }; return f(2); }"
    );
    assert(module.functions.length === 2, "module should have fe + anon");
    assertContains(printFunction(fn), "make_closure");
});

// --- lowering: exceptions ---------------------------------------------------------

test("lower: try/catch produces unwind edges into a catch block", () => {
    let { fn } = lowerOne(
        "function t(o) { try { o.f(); } catch (e) { return e; } return 1; }"
    );
    let printed = printFunction(fn);
    assertContains(printed, "unwind ^catch");
    assertContains(printed, "normal ^cont");
    assertContains(printed, ": exception):");

    let catch_bb = findBlock(fn, "catch");
    assert(catch_bb.isCatch, "catch block should be marked");
    assert(catch_bb.params[0]!.isException, "first catch param is the exception");
});

test("lower: throw inside try unwinds to the local handler", () => {
    let { fn } = lowerOne(
        "function th(c) { try { if (c) throw c; } catch (e) { return e; } return 0; }"
    );
    let printed = printFunction(fn);
    assertContains(printed, "throw");
    assertContains(printed, "unwind ^catch");
});

test("lower: variable state joins into the catch block per throw site", () => {
    let { fn } = lowerOne(
        "function j(o) { let x = 1; try { o.a(); x = 2; o.b(); } catch (e) { return x; } return x; }"
    );
    // catch reads x: its value differs by which call threw, so the catch
    // block needs a (non-exception) param joining 1 and 2.
    let catch_bb = findBlock(fn, "catch");
    assert(
        catch_bb.params.length === 2,
        `catch should have exception + x params, got ${catch_bb.params.length}`
    );
});

// --- lowering: misc ------------------------------------------------------------------

test("lower: new expressions become construct", () => {
    let { fn } = lowerOne("function nw(C) { return new C(1, 2); }");
    assertContains(printFunction(fn), "construct");
});

test("lower: array and object literals", () => {
    let { fn } = lowerOne("function lit() { return [1, 2, { a: 3, b: 4 }]; }");
    let printed = printFunction(fn);
    assertContains(printed, "make_array");
    assertContains(printed, 'make_object');
    assertContains(printed, 'keys=["a", "b"]');
});

test("lower: this expression", () => {
    let { fn } = lowerOne("function tt() { return this.x; }");
    assertContains(printFunction(fn), "get_prop_atom");
});

test("lower: unsupported constructs raise LowerNotSupported", () => {
    let threw = false;
    try {
        lowerFunctionNode(parseFn("function t(x) { with (x) { return y; } }"));
    } catch (e) {
        threw = isLowerNotSupported(e);
    }
    assert(threw, "expected LowerNotSupported");
});

// the remaining source-reachable LowerNotSupported guards.  everything
// else in scopes.js/lower.js is defensive: either the parser rejects the
// construct outright or a pre-EIR desugar pass removes it before EIR
// sees it.  these are the constructs a user can actually write that
// don't lower — each must fail loudly (there is no fallback pipeline).
test("lower: delete of a variable raises LowerNotSupported", () => {
    let threw = false;
    try {
        lowerFunctionNode(parseFn("function t(x) { delete x; return 1; }"));
    } catch (e) {
        threw = isLowerNotSupported(e);
    }
    assert(threw, "expected LowerNotSupported");
});

// --- lowering: per-iteration loop envs ---------------------------------------

test("lower: captured for-let var gets a per-iteration env", () => {
    let { module, fn } = lowerOne(
        "function f() { let fns = []; for (let i = 0; i < 3; i++) { fns.push(function () { return i; }); } return fns; }"
    );
    let printed = printFunction(fn);
    // an env is created at loop entry AND refreshed in the update block
    let update = findBlock(fn, "for_update");
    assert(
        update.insts.some((i) => i.op === "make_env"),
        "update block should make a fresh env"
    );
    // the header carries the current env as a block param
    assert(findBlock(fn, "for_header").params.length === 1, "header should carry the env");
    // the closure reads the loop var from its incoming env
    let child = findFn(module, "f.anon0");
    assertContains(printFunction(child), "env_load");
    verifyModule(module);
});

test("lower: uncaptured for-let vars stay SSA (no loop env)", () => {
    let { fn } = lowerOne(
        "function f(n) { let sum = 0; for (let i = 0; i < n; i++) { sum = sum + i; } return sum; }"
    );
    assert(
        !printFunction(fn).includes("make_env"),
        "no env expected for uncaptured loop vars"
    );
});

test("lower: captured for-of var gets a fresh env per iteration", () => {
    let { module, fn } = lowerOne(
        "function f(xs) { let fns = []; for (let x of xs) { fns.push(function () { return x; }); } return fns; }"
    );
    let body = findBlock(fn, "forof_body");
    assert(
        body.insts.some((i) => i.op === "make_env"),
        "body should make a fresh env each iteration"
    );
    verifyModule(module);
});

test("lower: for-of RHS closure capturing the loop var sees the loop env", () => {
    // scope analysis declares the binding before walking the RHS, so the
    // closure's incoming env must be the loop env (holding undefined at
    // that point — echojs has no TDZ), not the function env
    let { module, fn } = lowerOne(
        "function f(mk) { let fns = []; for (let x of mk(function () { return x; })) { fns.push(function () { return x; }); } return fns; }"
    );
    // an initial env exists before the RHS call
    const entry = fn.blocks[0]!;
    assert(
        entry.insts.some((i) => i.op === "make_env"),
        "entry should create the initial loop env before the RHS evaluates"
    );
    verifyModule(module);
});

test("lower: nested captured loops chain their envs", () => {
    let { module } = lowerOne(
        "function f(base) { let fns = []; for (let i = 0; i < 2; i++) { for (let j = 0; j < 2; j++) { fns.push(function () { return base + i + j; }); } } return fns; }"
    );
    verifyModule(module); // the env chain must verify (dominance + slots)
});

// --- lowering: %-intrinsics ------------------------------------------------

// parse + the pre-EIR desugar passes, like preEIRConvert in compile()
function parseFnPreEIR(src: string): e.FunctionDeclaration {
    let ast = esprima.parse(src, { loc: true, raw: true });
    const opts = { debug_passes: new Set<string>() } as CompilerOptions;
    ast = new DesugarClasses(opts).visit(ast) as e.Program;
    ast = new DesugarDestructuring(opts).visit(ast) as e.Program;
    ast = new DesugarGeneratorFunctions(opts).visit(ast) as e.Program;
    ast = new DesugarSpread(opts).visit(ast) as e.Program;
    ast = new DesugarMetaProperties(opts).visit(ast) as e.Program;
    for (const s of ast.body) if (s.type === "FunctionDeclaration") return s;
    throw new Error("no function declaration in source");
}
let parseFnSpreadDesugared = parseFnPreEIR;

test("lower: spread call lowers via %arrayFromSpread -> array_from_spread", () => {
    let r = lowerFunctionNode(parseFnSpreadDesugared("function f(a) { return g(1, 2, ...a); }"));
    verifyModule(r.module);
    let printed = printFunction(r.fn);
    assertContains(printed, "array_from_spread");
    assert(printed.indexOf('get_global atom="%') === -1, "intrinsic leaked as a global load");
});

test("lower: array literal spread lowers to array_from_spread", () => {
    let r = lowerFunctionNode(parseFnSpreadDesugared("function f(a, b) { return [0, ...a, ...b]; }"));
    verifyModule(r.module);
    assertContains(printFunction(r.fn), "array_from_spread");
});

test("lower: computed accessor keys lower via define_accessor_computed", () => {
    let r = lowerFunctionNode(
        parseFnPreEIR(
            "function t(k) { return { get [k]() { return 1; }, set [k](v) { this.v = v; } }; }"
        )
    );
    verifyModule(r.module);
    let printed = printFunction(r.fn);
    let first = printed.indexOf("define_accessor_computed");
    assert(first !== -1, "expected define_accessor_computed");
    assert(
        printed.indexOf("define_accessor_computed", first + 1) !== -1,
        "expected separate defines for getter and setter"
    );
});

test("lower: for-of pattern heads desugar pre-EIR", () => {
    let r = lowerFunctionNode(
        parseFnPreEIR("function f(ps) { let r = 0; for (let [a, b] of ps) r = r + a * b; return r; }")
    );
    verifyModule(r.module);
});

test("lower: catch parameter patterns desugar pre-EIR", () => {
    let r = lowerFunctionNode(
        parseFnPreEIR(
            "function f(g) { try { return g(); } catch ({ message }) { return message; } }"
        )
    );
    verifyModule(r.module);
});

test("lower: nested array spread lowers", () => {
    let r = lowerFunctionNode(
        parseFnPreEIR("function f(xs) { return [...[...xs, 5], 6]; }")
    );
    verifyModule(r.module);
    let printed = printFunction(r.fn);
    let first = printed.indexOf("array_from_spread");
    assert(first !== -1, "expected array_from_spread");
    assert(printed.indexOf("array_from_spread", first + 1) !== -1, "expected a second array_from_spread for the nested spread");
});

test("lower: debugger statement is a no-op", () => {
    let r = lowerFunctionNode(parseFn("function f() { debugger; return 1; }"));
    verifyModule(r.module);
});

test("lower: unknown %-intrinsics raise LowerNotSupported", () => {
    let fnNode = parseFnSpreadDesugared("function t(a) { return dummy(a); }");
    // synthesize a call to an intrinsic lowering doesn't know
    const retstmt = fnNode.body.body[0] as e.ReturnStatement;
    ((retstmt.argument as e.CallExpression).callee as e.Identifier).name = "%noSuchIntrinsic";
    let threw = false;
    try {
        lowerFunctionNode(fnNode);
    } catch (e) {
        threw = isLowerNotSupported(e);
    }
    assert(threw, "expected LowerNotSupported");
});

test("lower: derived class ctor lowers construct_super and rebinds this", () => {
    let r = lowerFunctionNode(
        parseFnPreEIR(
            "function f(v) { class A { constructor(x) { this.x = x; } } class B extends A { constructor() { super(1); this.v = v; } } return new B(); }"
        )
    );
    verifyModule(r.module);
    let ctor: Func | null = null;
    for (const fn of r.module.functions) if (/\.B$/.test(fn.name)) ctor = fn;
    assert(!!ctor, "expected the B constructor in the module");
    let printed = printFunction(ctor!);
    assertContains(printed, "construct_super");
    // this.v = v must store into construct_super's result, not the entry
    // this param (%1)
    assert(!/set_prop_atom %1,/.test(printed), "post-super `this` should be the rebound value");
});

test("lower: spread super call lowers to construct_super_apply", () => {
    let r = lowerFunctionNode(
        parseFnPreEIR(
            "function f() { class A { constructor(a, b) { this.s = a + b; } } class B extends A { constructor(xs) { super(...xs); } } return new B([1, 2]); }"
        )
    );
    verifyModule(r.module);
    let all = r.module.functions.map((fn) => printFunction(fn)).join("\n");
    assertContains(all, "construct_super_apply");
});

test("lower: new.target lowers to new_target", () => {
    let r = lowerFunctionNode(parseFnPreEIR("function f() { return new.target; }"));
    verifyModule(r.module);
    assertContains(printFunction(r.fn), "new_target");
});

test("lower: class accessors lower via make_object + defineProperties", () => {
    let r = lowerFunctionNode(
        parseFnPreEIR(
            "function f() { class T { get n() { return 1; } set n(v) { this._x = v; } } return new T(); }"
        )
    );
    verifyModule(r.module);
    let all = r.module.functions.map((fn) => printFunction(fn)).join("\n");
    // one property entry carrying BOTH accessors (the get/set pair shares
    // a make_object with keys get,set)
    assertContains(all, 'keys=["get", "set"]');
});

test("lower: array destructuring lowers via %createIteratorWrapper", () => {
    let r = lowerFunctionNode(
        parseFnPreEIR("function f(xs) { let [a, ...rest] = xs; return a + rest.length; }")
    );
    verifyModule(r.module);
    assertContains(printFunction(r.fn), 'name="iterator_wrapper_new"');
});

test("lower: pattern defaults lower as undefined checks", () => {
    let r = lowerFunctionNode(
        parseFnPreEIR("function f(o) { let { a = 5 } = o; return a; }")
    );
    verifyModule(r.module);
    let printed = printFunction(r.fn);
    assertContains(printed, "strict_eq");
    assertContains(printed, "get_prop_atom");
});

test("lower: unary void evaluates its argument and yields undefined", () => {
    let { fn } = lowerOne("function f(g) { return void g(); }");
    let printed = printFunction(fn);
    assertContains(printed, "call");
    assertContains(printed, 'kind="undefined"');
});

test("lower: arrow lexical this reads the owner's captured this", () => {
    let { module, fn } = lowerOne(
        "function f() { return function () { return [1].map(() => this.x); }; }"
    );
    verifyModule(module);
    // the method stores its this into an env; the arrow env_loads it
    let method: Func | null = null;
    for (const g of module.functions) if (/anon0$/.test(g.name)) method = g;
    assert(!!method, "expected the method in the module");
    assertContains(printFunction(method!), "env_store");
    let arrow: Func | null = null;
    for (const g of module.functions) if (/arrow1$/.test(g.name)) arrow = g;
    assert(!!arrow, "expected the arrow in the module");
    assertContains(printFunction(arrow!), "env_load");
});

test("lower: toplevel-arrow candidates using this still fall back", () => {
    const ast = esprima.parse("var f = () => this.x;", { loc: true, raw: true });
    const decl = ast.body[0] as e.VariableDeclaration;
    const arrow = decl.declarations[0]!.init as e.ArrowFunctionExpression;
    let threw = false;
    try {
        lowerFunctionNode(arrow, "f");
    } catch (e) {
        threw = isLowerNotSupported(e);
    }
    assert(threw, "expected LowerNotSupported");
});

test("lower: captured body-block let gets a per-iteration env", () => {
    let { module, fn } = lowerOne(
        "function f(n) { let fns = []; for (var i = 0; i < n; i++) { let j = i; fns.push(function () { return j; }); } return fns; }"
    );
    verifyModule(module);
    let body = findBlock(fn, "for_body");
    assert(
        body.insts.some((i) => i.op === "make_env"),
        "loop body should make a fresh env each iteration"
    );
});

test("lower: array holes stay holes", () => {
    let { fn } = lowerOne("function f() { return [, , 3]; }");
    let printed = printFunction(fn);
    assertContains(printed, "len=3");
});

test("lower: closures carry source-level display names", () => {
    let { fn } = lowerOne("function f() { return function inner() {}; }");
    assertContains(printFunction(fn), 'name="inner"');
});

test("scopes: same-named functions get distinct EIR names", () => {
    let r = lowerFunctionNode(
        parseFn(
            "function f() { function g() {} let o = { m: function g() {} }; return o.m || g; }"
        )
    );
    verifyModule(r.module);
    let names = r.module.functions.map((x) => x.name);
    assert(new Set(names).size === names.length, `duplicate names: ${names}`);
});

test("lower: labeled break/continue on nested loops", () => {
    let { module, fn } = lowerOne(
        "function f(g) { outer: for (let i = 0; i < 9; i++) { for (let j = 0; j < 9; j++) { if (g(i, j) < 0) break outer; if (g(i, j) > 9) continue outer; } } return 1; }"
    );
    verifyModule(module);
    assert(fn.blocks.length > 6, "expected nested loop CFG");
});

test("lower: labeled non-loop statement with break", () => {
    let { module, fn } = lowerOne(
        "function f(x) { let r = 0; done: { r = 1; if (x) break done; r = 2; } return r; }"
    );
    verifyModule(module);
    let labelBlock: Block | null = null;
    for (const blk of fn.blocks) if (blk.name.indexOf("label_done") === 0) labelBlock = blk;
    assert(!!labelBlock, "expected the label exit block");
});

test("lower: labeled continue through a finally runs the finalizer", () => {
    let { module, fn } = lowerOne(
        "function f(xs, log) { outer: for (let i = 0; i < xs.length; i++) { for (let j = 0; j < 2; j++) { try { if (xs[i] < 0) continue outer; } finally { log.push(j); } } } return log; }"
    );
    verifyModule(module);
    // finalizer duplication: the log.push lowers at least twice (normal
    // path + the labeled-continue path)
    let pushes = 0;
    fn.forEachInst((inst) => {
        if (inst.op === "get_prop_atom" && inst.imms.atom === "push") pushes++;
    });
    assert(pushes >= 2, `expected duplicated finalizer, saw ${pushes} push loads`);
});

test("lower: object-literal accessors define get/set pairs together", () => {
    let { module, fn } = lowerOne(
        "function f(v) { let o = { a: 1, get n() { return v; }, set n(x) { v = x; } }; return o; }"
    );
    verifyModule(module);
    let defines = 0;
    fn.forEachInst((inst) => {
        if (inst.op === "define_accessor") {
            defines++;
            assert(inst.imms.atom === "n", "accessor key");
        }
    });
    assert(defines === 1, `get/set pair must be ONE define_accessor, saw ${defines}`);
});

test("lower: tagged templates lower via template_callsite", () => {
    let { module, fn } = lowerOne("function f(tag, x) { return tag`a ${x} b`; }");
    verifyModule(module);
    let sites = 0;
    fn.forEachInst((inst) => {
        if (inst.op === "template_callsite") {
            sites++;
            assert((inst.imms["cooked"] as readonly string[]).length === 2, "two cooked strings");
        }
    });
    assert(sites === 1, `expected one callsite, saw ${sites}`);
});

test("lower: generator function lowers via make_generator/generator_yield", () => {
    let r = lowerFunctionNode(
        parseFnPreEIR("function f() { function* g() { yield 1; yield 2; } return g(); }")
    );
    verifyModule(r.module);
    let all = r.module.functions.map((fn) => printFunction(fn)).join("\n");
    assertContains(all, 'name="make_generator"');
    assertContains(all, 'name="generator_yield"');
});

test("lower: statement-position yield* lowers as a for-of delegate loop", () => {
    let r = lowerFunctionNode(
        parseFnPreEIR(
            "function f() { function* inner() { yield 1; } function* outer() { yield* inner(); } return outer(); }"
        )
    );
    verifyModule(r.module);
    let all = r.module.functions.map((fn) => printFunction(fn)).join("\n");
    assertContains(all, 'name="generator_yield"');
});

test("lower: program with several functions", () => {
    let ast = esprima.parse(
        "function one() { return 1; } function two() { return one() + 1; }",
        { loc: true, raw: true }
    );
    let mod = lowerProgram(ast, "twofns");
    assert(mod.functions.length === 2, "two functions");
    verifyModule(mod);
});

// --- verifier ------------------------------------------------------------------

test("verifier: rejects use that is not dominated by its def", () => {
    let fn = new Func("bad", []);
    let entry = fn.addBlock(new Block(fn, "entry"));
    let a_bb = fn.addBlock(new Block(fn, "a"));
    let b_bb = fn.addBlock(new Block(fn, "b"));
    entry.sealed = a_bb.sealed = b_bb.sealed = true;
    fn.entry = entry;

    let cond = new Inst(fn, "const", [], { kind: "boolean", value: true });
    cond.block = entry;
    entry.insts.push(cond);
    let cbr = new Inst(fn, "cond_br", [cond], {});
    cbr.block = entry;
    entry.insts.push(cbr);
    cbr.addTarget(a_bb, []);
    cbr.addTarget(b_bb, []);

    let c1 = new Inst(fn, "const", [], { kind: "number", value: 1 });
    c1.block = a_bb;
    a_bb.insts.push(c1);
    let ra = new Inst(fn, "return", [c1], {});
    ra.block = a_bb;
    a_bb.insts.push(ra);

    let bad = new Inst(fn, "strict_eq", [c1, c1], {});
    bad.block = b_bb;
    b_bb.insts.push(bad);
    let rb = new Inst(fn, "return", [bad], {});
    rb.block = b_bb;
    b_bb.insts.push(rb);

    let threw = false;
    try {
        verifyFunction(fn);
    } catch (e) {
        threw = /does not dominate/.test((e as Error).message);
    }
    assert(threw, "expected a dominance violation");
});

test("verifier: rejects unterminated blocks", () => {
    let fn = new Func("noterm", []);
    let entry = fn.addBlock(new Block(fn, "entry"));
    entry.sealed = true;
    let c = new Inst(fn, "const", [], { kind: "number", value: 1 });
    c.block = entry;
    entry.insts.push(c);

    let threw = false;
    try {
        verifyFunction(fn);
    } catch (e) {
        threw = /no terminator/.test((e as Error).message);
    }
    assert(threw, "expected a no-terminator error");
});

test("verifier: rejects normal edges into catch blocks", () => {
    let fn = new Func("badedge", []);
    let entry = fn.addBlock(new Block(fn, "entry"));
    let catch_bb = fn.addBlock(new Block(fn, "catch"));
    catch_bb.isCatch = true;
    let exc = catch_bb.addParam("%exception");
    exc.isException = true;
    entry.sealed = catch_bb.sealed = true;
    fn.entry = entry;

    let br = new Inst(fn, "br", [], {});
    br.block = entry;
    entry.insts.push(br);
    br.addTarget(catch_bb, []);

    let c = new Inst(fn, "const", [], { kind: "number", value: 0 });
    c.block = catch_bb;
    catch_bb.insts.push(c);
    let r = new Inst(fn, "return", [c], {});
    r.block = catch_bb;
    catch_bb.insts.push(r);

    let threw = false;
    try {
        verifyFunction(fn);
    } catch (e) {
        threw = /non-unwind edge into catch/.test((e as Error).message);
    }
    assert(threw, "expected a catch-edge violation");
});

// --- optimize: allocation sinking ----------------------------------------------

function assertNotContains(haystack: string, needle: string): void {
    if (haystack.indexOf(needle) !== -1)
        throw new Error(`expected output to NOT contain '${needle}'\n---\n${haystack}\n---`);
}

function lowerAndOptimize(src: string): { fn: Func; printed: string } {
    let { fn } = lowerOne(src);
    optimizeFunction(fn);
    verifyFunction(fn);
    return { fn, printed: printFunction(fn) };
}

test("optimize: non-escaping object literal reads fold and the alloc dies", () => {
    let { printed } = lowerAndOptimize("function f() { let o = { a: 1, b: 2 }; return o.a + o.b; }");
    assertNotContains(printed, "make_object");
    assertNotContains(printed, "get_prop_atom");
});

test("optimize: duplicate literal keys fold to the last definition", () => {
    let { fn, printed } = lowerAndOptimize("function f() { let o = { a: 1, a: 2 }; return o.a; }");
    assertNotContains(printed, "make_object");
    // the surviving return operand should be the const 2
    let ret: Inst | null = null;
    fn.forEachInst((i) => { if (i.op === "return") ret = i; });
    assert(ret!.operands[0]!.imms.value === 2, "expected the second definition's value");
});

test("optimize: escaping object literal is untouched", () => {
    let { printed } = lowerAndOptimize("function f(g) { let o = { a: 1 }; g(o); return o.a; }");
    assertContains(printed, "make_object");
    assertContains(printed, 'get_prop_atom');
});

test("optimize: write-only object literal dies with its stores", () => {
    let { printed } = lowerAndOptimize("function f(x) { let o = { a: 1 }; o.a = x; return x; }");
    assertNotContains(printed, "make_object");
    assertNotContains(printed, "set_prop_atom");
});

test("optimize: a written key's reads fold flow-sensitively (sinking-P3)", () => {
    // the read after the write sees the written value; the store and
    // the allocation drain
    let { fn, printed } = lowerAndOptimize(
        "function f(x) { let o = { a: 1 }; o.a = x; return o.a; }"
    );
    assertNotContains(printed, "make_object");
    assertNotContains(printed, "get_prop_atom");
    assertNotContains(printed, "set_prop_atom");
    let ret: Inst | null = null;
    fn.forEachInst((i) => { if (i.op === "return") ret = i; });
    assert(ret!.operands[0]!.op === "blockparam", "return should see the written param x");
});

test("optimize: EJS_NO_FLOW_SINK restores the written-key decline", () => {
    process.env["EJS_NO_FLOW_SINK"] = "1";
    try {
        let { printed } = lowerAndOptimize(
            "function f(x) { let o = { a: 1 }; o.a = x; return o.a; }"
        );
        assertContains(printed, "make_object");
        assertContains(printed, "get_prop_atom");
    } finally {
        delete process.env["EJS_NO_FLOW_SINK"];
    }
});

test("optimize: non-own-key read keeps the object (prototype chain)", () => {
    let { printed } = lowerAndOptimize("function f() { let o = { a: 1 }; return o.toString; }");
    assertContains(printed, "make_object");
});

test("optimize: array literal const-index and length reads fold", () => {
    let { printed } = lowerAndOptimize("function f() { let a = [10, 20, 30]; return a[0] + a.length; }");
    assertNotContains(printed, "make_array");
    assertNotContains(printed, "get_prop");
});

test("optimize: array hole reads keep the array", () => {
    let { printed } = lowerAndOptimize("function f() { let a = [1, , 3]; return a[1]; }");
    assertContains(printed, "make_array");
});

test("optimize: out-of-range array read keeps the array", () => {
    let { printed } = lowerAndOptimize("function f() { let a = [1]; return a[5]; }");
    assertContains(printed, "make_array");
});

test("optimize: computed non-const array read keeps the array", () => {
    let { printed } = lowerAndOptimize("function f(i) { let a = [1, 2]; return a[i]; }");
    assertContains(printed, "make_array");
});

test("optimize: array method call keeps the array", () => {
    let { printed } = lowerAndOptimize("function f() { let a = [1, 2]; return a.join(','); }");
    assertContains(printed, "make_array");
});

test("optimize: nested literal sinks once the outer one dies", () => {
    let { printed } = lowerAndOptimize(
        "function f() { let o = { inner: { x: 7 } }; return o.inner.x; }"
    );
    assertNotContains(printed, "make_object");
});

test("optimize: object flowing into a block param is an escape", () => {
    let { printed } = lowerAndOptimize(
        "function f(c) { let o = c ? { a: 1 } : { a: 2 }; return o.a; }"
    );
    assertContains(printed, "make_object");
});

test("optimize: reads inside try (unwind targets) are left alone", () => {
    let { printed } = lowerAndOptimize(
        "function f() { let o = { a: 1 }; try { return o.a; } catch (e) { return 0; } }"
    );
    assertContains(printed, "make_object");
    assertContains(printed, "get_prop_atom");
});

test("optimize: single-block IIFE inlines and its env scalar-replaces", () => {
    let { module, fn } = lowerOne(
        "function f(x) { let r = ((a) => a + 1)(x); return r; }"
    );
    optimizeFunction(fn, module);
    verifyFunction(fn);
    let printed = printFunction(fn);
    assertNotContains(printed, "make_closure");
    assertNotContains(printed, "call");
    assertContains(printed, "add");
});

test("optimize: escaping closure is not inlined", () => {
    let { module, fn } = lowerOne(
        "function f(g) { let h = (a) => a + 1; g(h); return h(2); }"
    );
    optimizeFunction(fn, module);
    verifyFunction(fn);
    assertContains(printFunction(fn), "make_closure");
});

test("optimize: same-block env with loads and stores scalar-replaces", () => {
    // the arrow captures x, forcing x into an env; after inlining, the
    // env ops are all in one block and dissolve
    let { module, fn } = lowerOne(
        "function f(x) { let get = () => x; return get(); }"
    );
    optimizeFunction(fn, module);
    verifyFunction(fn);
    let printed = printFunction(fn);
    assertNotContains(printed, "make_env");
    assertNotContains(printed, "env_load");
    assertNotContains(printed, "call");
});

test("optimize: destructuring swap dissolves to pure SSA", () => {
    let r = lowerFunctionNode(
        parseFnPreEIR("function f(a, b) { [a, b] = [b, a]; return a - b; }")
    );
    optimizeFunction(r.fn, r.module);
    verifyFunction(r.fn);
    let printed = printFunction(r.fn);
    assertNotContains(printed, "make_env");
    assertNotContains(printed, "make_closure");
    assertNotContains(printed, "make_array");
    assertNotContains(printed, "iterator_wrapper_new");
    assertNotContains(printed, "call");
});

test("optimize: iterator walk over a literal folds, short RHS pads undefined", () => {
    let r = lowerFunctionNode(
        parseFnPreEIR("function f(x) { let [a, b, c] = [x, 2]; return [a, b, c].length && a + b + (c === undefined); }")
    );
    optimizeFunction(r.fn, r.module);
    verifyFunction(r.fn);
    let printed = printFunction(r.fn);
    assertNotContains(printed, "iterator_wrapper_new");
    assertNotContains(printed, 'atom="getNextValue"');
});

test("optimize: iterator walk over a non-literal keeps the runtime protocol", () => {
    let r = lowerFunctionNode(
        parseFnPreEIR("function f(xs) { let [a, b] = xs; return a + b; }")
    );
    optimizeFunction(r.fn, r.module);
    verifyFunction(r.fn);
    assertContains(printFunction(r.fn), 'name="iterator_wrapper_new"');
});

test("optimize: rest pattern (getRest) keeps the runtime protocol", () => {
    let r = lowerFunctionNode(
        parseFnPreEIR("function f(x, y) { let [a, ...rest] = [x, y, 3]; return a + rest.length; }")
    );
    optimizeFunction(r.fn, r.module);
    verifyFunction(r.fn);
    assertContains(printFunction(r.fn), 'name="iterator_wrapper_new"');
});

test("optimize: array with another use keeps the iterator walk", () => {
    let r = lowerFunctionNode(
        parseFnPreEIR("function f(x, y) { let arr = [x, y]; let [a] = arr; return a + arr.length; }")
    );
    optimizeFunction(r.fn, r.module);
    verifyFunction(r.fn);
    assertContains(printFunction(r.fn), 'name="iterator_wrapper_new"');
});

test("optimize: env read from a later block is left alone", () => {
    // the loop body reads the env across blocks: not same-block, no sink
    let { module, fn } = lowerOne(
        "function f(x, n) { let get = () => x; while (n) { n = n - get(); } return n; }"
    );
    optimizeFunction(fn, module);
    verifyFunction(fn);
});

test("optimize: DCE removes unused pure chains but keeps effects", () => {
    let { printed } = lowerAndOptimize(
        "function f(x) { let unused = { a: 1 }; let kept = x.y; return 5; }"
    );
    assertNotContains(printed, "make_object");
    // x.y may have observable effects (getter) and must survive
    assertContains(printed, "get_prop_atom");
});

// --- the typed low tier ------------------------------------------------

function assertVerifyFails(fn: Func, needle: string): void {
    try {
        verifyFunction(fn);
    } catch (err) {
        const msg = (err as Error).message;
        if (msg.indexOf(needle) === -1)
            throw new Error(`verifier failed, but with '${msg}' (wanted '${needle}')`);
        return;
    }
    throw new Error(`verifier accepted an ill-typed function (wanted '${needle}')`);
}

test("lowtier: printer shows typed defs; untyped stay bare", () => {
    const printed = printFunction(buildLowTierAdd("probe"));
    assertContains(printed, ': i1 = has_tag');
    assertContains(printed, 'tag="number"');
    assertContains(printed, ": f64 = unbox_f64");
    assertContains(printed, ": f64 = f64_add");
    assertNotContains(printed, ": any ="); // "any" defs print bare
    // box_f64 produces a boxed value again: no type annotation
    const boxline = printed.split("\n").filter((l) => l.indexOf("box_f64") !== -1 && l.indexOf("unbox") === -1)[0]!;
    assert(boxline.indexOf(": f64") === -1 && boxline.indexOf(": i1") === -1, "box_f64 def must be untyped");
});

test("lowtier: the parameterized diamond covers sub/mul/div", () => {
    for (const [f64op, generic] of [["f64_sub", "sub"], ["f64_mul", "mul"], ["f64_div", "div"]] as const) {
        const fn = buildArithDiamond("probe_" + generic, f64op, generic);
        verifyFunction(fn);
        const printed = printFunction(fn);
        assertContains(printed, ": f64 = " + f64op);
        assertContains(printed, generic + " ");
    }
});

test("lowtier: f64_lt prints as i1 and feeds cond_br", () => {
    const printed = printFunction(buildLowTierLt("probe"));
    assertContains(printed, ": i1 = f64_lt");
    assertContains(printed, ": f64 = unbox_f64");
});

test("lowtier: verifier accepts the guarded diamonds", () => {
    verifyFunction(buildLowTierAdd("ok_add")); // builders verify internally too
    verifyFunction(buildLowTierLt("ok_lt"));
});

test("lowtier: verifier rejects f64 flowing into a generic op", () => {
    const fb = new FunctionBuilder("bad", ["%env", "%this", "a"]);
    const a = fb.readVariable("a", fb.cur);
    const ua = fb.emit("unbox_f64", [a], {});
    fb.ret(fb.emit("add", [ua, a], {}));
    assertVerifyFails(fb.finish(), "may not be f64");
});

test("lowtier: verifier rejects a boxed value in an f64 operand slot", () => {
    const fb = new FunctionBuilder("bad", ["%env", "%this", "a"]);
    const a = fb.readVariable("a", fb.cur);
    const sum = fb.emit("f64_add", [a, a], {});
    fb.ret(fb.emit("box_f64", [sum], {}));
    assertVerifyFails(fb.finish(), "wants f64, got any");
});

test("lowtier: verifier rejects i1 where a boxed value is expected", () => {
    const fb = new FunctionBuilder("bad", ["%env", "%this", "a"]);
    const a = fb.readVariable("a", fb.cur);
    const t = fb.emit("has_tag", [a], { tag: "number" });
    fb.ret(t);
    assertVerifyFails(fb.finish(), "may not be i1");
});

test("lowtier: verifier rejects an i1 operand to an f64-typed op", () => {
    const fb = new FunctionBuilder("bad", ["%env", "%this", "a"]);
    const a = fb.readVariable("a", fb.cur);
    const t = fb.emit("has_tag", [a], { tag: "number" });
    fb.ret(fb.emit("box_f64", [t], {}));
    assertVerifyFails(fb.finish(), "wants f64, got i1");
});

test("lowtier: verifier rejects raw f64/i1 block arguments", () => {
    const fb = new FunctionBuilder("bad", ["%env", "%this", "a"]);
    const a = fb.readVariable("a", fb.cur);
    const ua = fb.emit("unbox_f64", [a], {});
    const join = fb.newBlock("join");
    const jp = join.addParam("jp");
    fb.br(join, [ua]);
    fb.sealBlock(join);
    fb.setInsertPoint(join);
    fb.ret(jp);
    assertVerifyFails(fb.finish(), "block arguments must be boxed");
});

test("lowtier: cond_br accepts i1 and legacy any conditions, rejects f64", () => {
    const fb = new FunctionBuilder("bad", ["%env", "%this", "a"]);
    const a = fb.readVariable("a", fb.cur);
    const ua = fb.emit("unbox_f64", [a], {});
    const t = fb.newBlock("t");
    const f = fb.newBlock("f");
    fb.condBr(ua, t, [], f, []);
    fb.sealBlock(t);
    fb.sealBlock(f);
    fb.setInsertPoint(t);
    fb.ret(fb.constUndefined());
    fb.setInsertPoint(f);
    fb.ret(fb.constUndefined());
    assertVerifyFails(fb.finish(), "cond_br condition may not be f64");
});

test("lowtier: DCE removes dead pure low-tier chains", () => {
    const fb = new FunctionBuilder("deadchain", ["%env", "%this", "a"]);
    const a = fb.readVariable("a", fb.cur);
    const ua = fb.emit("unbox_f64", [a], {});
    const s = fb.emit("f64_add", [ua, ua], {});
    fb.emit("box_f64", [s], {}); // dead: result unused (GC effect is removable)
    fb.ret(a);
    const fn = fb.finish();
    verifyFunction(fn);
    const module = new Module("m");
    module.functions.push(fn);
    optimizeFunction(fn, module);
    verifyFunction(fn);
    const printed = printFunction(fn);
    assertNotContains(printed, "f64_add");
    assertNotContains(printed, "box_f64");
    assertNotContains(printed, "unbox_f64");
});

// --- oracle-guided guarded arithmetic ------------------------------------

// a hand-built TypeOracle: types Identifier nodes by name, everything else
// (and unknown names) is top.  The TypeOracle interface from Chunk G is
// all lowering may consume, so this is a faithful stand-in for maam.
function stubOracle(types: Record<string, TypeTag[] | undefined>): TypeOracle {
    return {
        typeOfNode: (n) => {
            const id = n as { type?: string; name?: string };
            const tags = id.type === "Identifier" && id.name !== undefined ? types[id.name] : undefined;
            return tags ? { tags: new Set(tags) } : { tags: "top" };
        },
        closedWorld: () => false,
        describe: () => "stub",
    };
}

function lowerWithOracle(src: string, oracle: TypeOracle | null) {
    let r = lowerFunctionNode(parseFn(src), undefined, oracle);
    verifyModule(r.module); // (g) every lowered output must verify
    return { printed: printFunction(r.fn), diamonds: r.diamonds };
}

const DIAMOND_MARKS = ["has_tag", "unbox_f64", "box_f64", "num_join"];

test("typed-arith: {number}x{number} + emits the guarded diamond", () => {
    const { printed, diamonds } = lowerWithOracle(
        "function f(x, y) { return x + y; }",
        stubOracle({ x: ["number"], y: ["number"] })
    );
    assert(diamonds === 1, `diamonds=${diamonds}`);
    for (const m of DIAMOND_MARKS) assertContains(printed, m);
    assertContains(printed, 'tag="number"');
    assertContains(printed, ": f64 = f64_add");
    assertContains(printed, ": f64 = unbox_f64");
    assertContains(printed, ": i1 = has_tag");
});

test("typed-arith: null oracle lowers exactly as before (no diamond)", () => {
    const { printed, diamonds } = lowerWithOracle("function f(x, y) { return x + y; }", null);
    assert(diamonds === 0, `diamonds=${diamonds}`);
    for (const m of DIAMOND_MARKS) assertNotContains(printed, m);
    assertContains(printed, "add ");
});

test("typed-arith: string operands take no diamond", () => {
    const { printed, diamonds } = lowerWithOracle(
        "function f(x, y) { return x + y; }",
        stubOracle({ x: ["string"], y: ["string"] })
    );
    assert(diamonds === 0, `diamonds=${diamonds}`);
    assertNotContains(printed, "has_tag");
});

test("typed-arith: mixed, top, and widened number|undefined take no diamond", () => {
    for (const types of [
        { x: ["number"] as TypeTag[], y: ["string"] as TypeTag[] }, // mixed
        { x: ["number"] as TypeTag[], y: undefined }, // top
        { x: ["number", "undefined"] as TypeTag[], y: ["number"] as TypeTag[] }, // widened
    ]) {
        const { printed, diamonds } = lowerWithOracle(
            "function f(x, y) { return x + y; }",
            stubOracle(types)
        );
        assert(diamonds === 0, `diamonds=${diamonds} for ${JSON.stringify(types)}`);
        assertNotContains(printed, "has_tag");
    }
});

test("typed-arith: numeric literals type directly — `x + 1` diamonds", () => {
    const { printed, diamonds } = lowerWithOracle(
        "function f(x) { return x + 1; }",
        stubOracle({ x: ["number"] })
    );
    assert(diamonds === 1, `diamonds=${diamonds}`);
    assertContains(printed, ": f64 = f64_add");
    // and a negated literal too (parsed as unary minus over a literal)
    const neg = lowerWithOracle("function f(x) { return x - -2; }", stubOracle({ x: ["number"] }));
    assert(neg.diamonds === 1, `diamonds=${neg.diamonds}`);
    assertContains(neg.printed, ": f64 = f64_sub");
});

test("typed-arith: literals alone do not diamond without an oracle", () => {
    const { printed, diamonds } = lowerWithOracle("function f() { return 1 + 2; }", null);
    assert(diamonds === 0, `diamonds=${diamonds}`);
    assertNotContains(printed, "has_tag");
});

test("typed-arith: `<` diamonds through boolean-constant join edges", () => {
    const { printed, diamonds } = lowerWithOracle(
        "function f(x, y) { return x < y; }",
        stubOracle({ x: ["number"], y: ["number"] })
    );
    assert(diamonds === 1, `diamonds=${diamonds}`);
    assertContains(printed, ": i1 = f64_lt");
    assertContains(printed, "num_lt_true");
    assertContains(printed, "num_lt_false");
    // the i1 never reaches the join: its edges carry boolean constants
    assertContains(printed, 'kind="boolean", value=true');
    assertContains(printed, 'kind="boolean", value=false');
    assertNotContains(printed, "= box_f64"); // no f64 result to box for `<` (unbox_f64 remains)
});

test("typed-arith: mul/div diamonds carry their ops", () => {
    for (const [src, op] of [
        ["function f(x, y) { return x * y; }", "f64_mul"],
        ["function f(x, y) { return x / y; }", "f64_div"],
    ] as const) {
        const { printed, diamonds } = lowerWithOracle(src, stubOracle({ x: ["number"], y: ["number"] }));
        assert(diamonds === 1, `diamonds=${diamonds}`);
        assertContains(printed, ": f64 = " + op);
    }
});

// --- guard-region merging + raw f64 joins ------------------------------

// like the real maam oracle, this types the named identifiers as
// {number} AND any arithmetic expression whose operands are typed —
// hypot2's `a*a + b*b` is three diamonds only because the outer add's
// BinaryExpression operands type as {number} too
function numericStubOracle(names: string[]): TypeOracle {
    const numeric = (n: unknown): boolean => {
        const node = n as {
            type?: string;
            name?: string;
            operator?: string;
            value?: unknown;
            left?: unknown;
            right?: unknown;
        };
        if (node.type === "Identifier") return names.indexOf(node.name!) !== -1;
        if (node.type === "Literal") return typeof node.value === "number";
        if (
            node.type === "BinaryExpression" &&
            (node.operator === "+" || node.operator === "-" || node.operator === "*" || node.operator === "/")
        )
            return numeric(node.left) && numeric(node.right);
        return false;
    };
    return {
        typeOfNode: (n) => (numeric(n) ? { tags: new Set<TypeTag>(["number"]) } : { tags: "top" }),
        closedWorld: () => false,
        describe: () => "numeric-stub",
    };
}

function lowerOptWithOracle(src: string, oracle: TypeOracle | null): { fn: Func; printed: string } {
    let r = lowerFunctionNode(parseFn(src), undefined, oracle);
    verifyModule(r.module);
    optimizeFunction(r.fn, r.module);
    verifyFunction(r.fn);
    return { fn: r.fn, printed: printFunction(r.fn) };
}

function countOps(fn: Func, op: string): number {
    let n = 0;
    fn.forEachInst((i) => {
        if (i.op === op) n++;
    });
    return n;
}

function guardFalseTargets(fn: Func): Set<Block> {
    const targets = new Set<Block>();
    fn.forEachInst((i) => {
        if (i.op === "cond_br" && i.operands[0]!.op === "has_tag") targets.add(i.targets![1]!.block);
    });
    return targets;
}

test("guard-fold: x * x re-tests x only once", () => {
    const { fn } = lowerOptWithOracle("function f(x) { return x * x; }", numericStubOracle(["x"]));
    assert(countOps(fn, "has_tag") === 1, `has_tag = ${countOps(fn, "has_tag")}`);
});

test("guard-merge: hypot2 becomes one guard region with one slow path", () => {
    // as lowered this is three diamonds / six has_tags (see the guarded-arithmetic
    // dump); merged: one has_tag per distinct value, one slow path
    const { fn } = lowerOptWithOracle(
        "function hypot2(a, b) { return a * a + b * b; }",
        numericStubOracle(["a", "b"])
    );
    assert(countOps(fn, "has_tag") === 2, `has_tag = ${countOps(fn, "has_tag")}`);
    const ft = guardFalseTargets(fn);
    assert(ft.size === 1, `guard-failure targets = ${ft.size}`);
    // the full generic computation survives on the (single) slow path
    assert(countOps(fn, "mul") === 2 && countOps(fn, "add") === 1, "generic ops must survive");
});

test("guard-merge: merged fast region is unboxed end-to-end, boxing once", () => {
    const { fn, printed } = lowerOptWithOracle(
        "function hypot2(a, b) { return a * a + b * b; }",
        numericStubOracle(["a", "b"])
    );
    // exactly one box at the region exit; only the region INPUTS unbox
    assert(countOps(fn, "box_f64") === 1, `box_f64 = ${countOps(fn, "box_f64")}`);
    assert(countOps(fn, "unbox_f64") === 4, `unbox_f64 = ${countOps(fn, "unbox_f64")}`);
    // intermediate joins carry raw f64 params (the optimizer-scoped lift
    // of the P2 boxed-edges rule), all marked for the verifier
    let rawParams = 0;
    fn.forEachInst((i) => {
        if (i.op === "blockparam" && i.type === "f64") {
            assert(i.rawJoin, "f64 param must carry the rawJoin marker");
            rawParams++;
        }
    });
    assert(rawParams >= 2, `expected f64 join params, got ${rawParams}`);
    assertContains(printed, ": f64):"); // an intermediate join's param list
});

test("guard-merge: statement chains merge across pure prefixes (bench kernel)", () => {
    // i*i, s+_, i/2 (const-operand diamond), -, i+1, s+i: six diamonds,
    // two distinct guarded values, const guards fold, one slow path
    const { fn } = lowerOptWithOracle(
        "function k(s, i) { s = s + i * i - i / 2; i = i + 1; return s + i; }",
        numericStubOracle(["s", "i"])
    );
    assert(countOps(fn, "has_tag") === 2, `has_tag = ${countOps(fn, "has_tag")}`);
    const ft = guardFalseTargets(fn);
    assert(ft.size === 1, `guard-failure targets = ${ft.size}`);
    assert(countOps(fn, "box_f64") === 1, `box_f64 = ${countOps(fn, "box_f64")}`);
});

test("guard-merge: a non-dominating guard is neither folded nor merged", () => {
    // D1 lives in the then-branch: its guards do NOT dominate the second
    // x+y after the if-join, so nothing may fold or merge
    const { fn } = lowerOptWithOracle(
        "function f(c, x, y) { var t = 0; if (c) { t = x + y; } var w = x + y; return t + w; }",
        numericStubOracle(["x", "y"])
    );
    assert(countOps(fn, "has_tag") === 4, `has_tag = ${countOps(fn, "has_tag")}`);
    const ft = guardFalseTargets(fn);
    assert(ft.size === 2, `guard-failure targets = ${ft.size}`);
    // both regions still rejoin boxed: no raw params anywhere
    let rawParams = 0;
    fn.forEachInst((i) => {
        if (i.op === "blockparam" && i.type === "f64") rawParams++;
    });
    assert(rawParams === 0, `expected no f64 params, got ${rawParams}`);
    assert(countOps(fn, "box_f64") === 2, `box_f64 = ${countOps(fn, "box_f64")}`);
});

test("guard-merge: `<` diamonds still verify and keep their shape through opt", () => {
    const { fn } = lowerOptWithOracle(
        "function f(x, y) { return x < y; }",
        stubOracle({ x: ["number"], y: ["number"] })
    );
    assert(countOps(fn, "f64_lt") === 1, "lt fast path survives");
    assert(countOps(fn, "lt") === 1, "lt slow path survives");
});

// hand-build one guarded diamond: head cond_br(has_tag v) -> fast|slow,
// fast unbox/f64_mul/box, slow mul(sl, sr), join(param).  Returns the
// pieces the attacks need to vary.
function buildDiamond(
    fb: FunctionBuilder,
    v: Inst,
    slowL: Inst,
    slowR: Inst,
    name: string
): { join: Block; param: Inst; slowOp: Inst } {
    const fast = fb.newBlock(name + "_fast");
    const slow = fb.newBlock(name + "_slow");
    const join = fb.newBlock(name + "_join");
    const param = join.addParam(name + "_p");
    const t = fb.emit("has_tag", [v], { tag: "number" });
    fb.condBr(t, fast, [], slow, []);
    fb.sealBlock(fast);
    fb.sealBlock(slow);
    fb.setInsertPoint(fast);
    const u = fb.emit("unbox_f64", [v], {});
    fb.br(join, [fb.emit("box_f64", [fb.emit("f64_mul", [u, u], {})], {})]);
    fb.setInsertPoint(slow);
    const slowOp = fb.emit("mul", [slowL, slowR], {});
    fb.br(join, [slowOp]);
    fb.sealBlock(join);
    fb.setInsertPoint(join);
    return { join: join, param: param, slowOp: slowOp };
}

test("guard-merge: a foreign edge into region1's join refuses the merge (attack A)", () => {
    // entry picks region1 or a FOREIGN edge handing j1 the unrelated
    // value c.  Merging would substitute region2's slow operands with
    // region1's slow values — wrong on the foreign path.  Must refuse.
    const fb = new FunctionBuilder("attack_a", ["%env", "%this", "a", "c", "d"]);
    const a = fb.readVariable("a", fb.cur);
    const c = fb.readVariable("c", fb.cur);
    const d = fb.readVariable("d", fb.cur);
    const head1 = fb.newBlock("head1");
    const jfor = fb.newBlock("jfor");
    const td = fb.emit("has_tag", [d], { tag: "number" });
    fb.condBr(td, head1, [], jfor, []);
    fb.sealBlock(head1);
    fb.sealBlock(jfor);
    fb.setInsertPoint(head1);
    const fast1 = fb.newBlock("fast1");
    const slow1 = fb.newBlock("slow1");
    const j1 = fb.newBlock("j1");
    const p = j1.addParam("p");
    const t1 = fb.emit("has_tag", [a], { tag: "number" });
    fb.condBr(t1, fast1, [], slow1, []);
    fb.sealBlock(fast1);
    fb.sealBlock(slow1);
    fb.setInsertPoint(fast1);
    const ua = fb.emit("unbox_f64", [a], {});
    fb.br(j1, [fb.emit("box_f64", [fb.emit("f64_mul", [ua, ua], {})], {})]);
    fb.setInsertPoint(slow1);
    const m = fb.emit("mul", [a, a], {});
    fb.br(j1, [m]);
    // the foreign edge, bypassing region1 entirely
    fb.setInsertPoint(jfor);
    fb.br(j1, [c]);
    fb.sealBlock(j1);
    fb.setInsertPoint(j1);
    const r2 = buildDiamond(fb, p, p, p, "r2");
    fb.ret(r2.param);
    const fn = fb.finish();
    verifyFunction(fn);
    const stats = optimizeFunction(fn);
    verifyFunction(fn);
    assert(stats.regions_merged === 0, `merge must be refused, got ${stats.regions_merged}`);
    assert(r2.slowOp.operands[0] === p && r2.slowOp.operands[1] === p, "slow operands untouched");
});

test("guard-merge: a non-twin slow arm refuses the merge (attack F)", () => {
    // region2's fast arm computes p*p but its slow arm computes
    // mul(p, e).  Pre-merge the region1-slow route passes region2's
    // guard (mul results are numbers) and takes the FAST arm; the merge
    // would reroute it through the non-twin slow arm.  Must refuse.
    const fb = new FunctionBuilder("attack_f", ["%env", "%this", "a", "e"]);
    const a = fb.readVariable("a", fb.cur);
    const e = fb.readVariable("e", fb.cur);
    const r1 = buildDiamond(fb, a, a, a, "r1");
    const r2 = buildDiamond(fb, r1.param, r1.param, e, "r2"); // slow: mul(p, e) — NOT the twin
    fb.ret(r2.param);
    const fn = fb.finish();
    verifyFunction(fn);
    const stats = optimizeFunction(fn);
    verifyFunction(fn);
    assert(stats.regions_merged === 0, `merge must be refused, got ${stats.regions_merged}`);
});

test("guard-merge: a non-twin REGION1 slow arm refuses the merge (attack G)", () => {
    // the mirror of attack F: region1's fast arm computes a*a but its
    // slow arm computes mul(a, b) with BOTH operands guard-proven (so
    // the re-execution purity check alone would pass); region2 is an
    // honest twin on an unrelated c.  Post-merge, a c-guard failure
    // after region1's fast arm would reroute through region1's non-twin
    // slow arm: (a*b)^2 instead of (a*a)^2.  Must refuse.
    const fb = new FunctionBuilder("attack_g", ["%env", "%this", "a", "b", "c"]);
    const a = fb.readVariable("a", fb.cur);
    const b = fb.readVariable("b", fb.cur);
    const c = fb.readVariable("c", fb.cur);
    const g2 = fb.newBlock("g2");
    const fast1 = fb.newBlock("fast1");
    const slow1 = fb.newBlock("slow1");
    const j1 = fb.newBlock("j1");
    const p = j1.addParam("p");
    const t1 = fb.emit("has_tag", [a], { tag: "number" });
    fb.condBr(t1, g2, [], slow1, []);
    fb.sealBlock(g2);
    fb.setInsertPoint(g2);
    const t1b = fb.emit("has_tag", [b], { tag: "number" });
    fb.condBr(t1b, fast1, [], slow1, []);
    fb.sealBlock(fast1);
    fb.sealBlock(slow1);
    fb.setInsertPoint(fast1);
    const ua = fb.emit("unbox_f64", [a], {});
    fb.br(j1, [fb.emit("box_f64", [fb.emit("f64_mul", [ua, ua], {})], {})]); // a*a
    fb.setInsertPoint(slow1);
    const m = fb.emit("mul", [a, b], {}); // NOT the twin; operands both guard-proven
    fb.br(j1, [m]);
    fb.sealBlock(j1);
    fb.setInsertPoint(j1);
    // region2: guard the unrelated c, both arms honestly compute p*p
    const fast2 = fb.newBlock("fast2");
    const slow2 = fb.newBlock("slow2");
    const j2 = fb.newBlock("j2");
    const q = j2.addParam("q");
    const t2 = fb.emit("has_tag", [c], { tag: "number" });
    fb.condBr(t2, fast2, [], slow2, []);
    fb.sealBlock(fast2);
    fb.sealBlock(slow2);
    fb.setInsertPoint(fast2);
    const up = fb.emit("unbox_f64", [p], {});
    fb.br(j2, [fb.emit("box_f64", [fb.emit("f64_mul", [up, up], {})], {})]);
    fb.setInsertPoint(slow2);
    const n = fb.emit("mul", [p, p], {});
    fb.br(j2, [n]);
    fb.sealBlock(j2);
    fb.setInsertPoint(j2);
    fb.ret(q);
    const fn = fb.finish();
    verifyFunction(fn);
    const stats = optimizeFunction(fn);
    verifyFunction(fn);
    assert(stats.regions_merged === 0, `merge must be refused, got ${stats.regions_merged}`);
    assert(n.operands[0] === p && n.operands[1] === p, "slow operands untouched");
});

// attack-H family: region1 an honest twin on `a`; region2 guards its
// param p and multiplies p by a const materialized SEPARATELY in each
// arm.  With corresponding consts the merge must fire; with +0 vs -0 it
// must refuse (sign of zero is observable via 1/x).
function buildConstPairShape(
    fastConst: number,
    slowConst: number
): { fn: Func; stats: OptStats } {
    const fb = new FunctionBuilder("constpair", ["%env", "%this", "a"]);
    const a = fb.readVariable("a", fb.cur);
    const r1 = buildDiamond(fb, a, a, a, "r1");
    const p = r1.param;
    const fast2 = fb.newBlock("fast2");
    const slow2 = fb.newBlock("slow2");
    const j2 = fb.newBlock("j2");
    const q = j2.addParam("q");
    const t2 = fb.emit("has_tag", [p], { tag: "number" });
    fb.condBr(t2, fast2, [], slow2, []);
    fb.sealBlock(fast2);
    fb.sealBlock(slow2);
    fb.setInsertPoint(fast2);
    const cf = fb.emit("const", [], { kind: "number", value: fastConst });
    const uc = fb.emit("unbox_f64", [cf], {});
    const up = fb.emit("unbox_f64", [p], {});
    fb.br(j2, [fb.emit("box_f64", [fb.emit("f64_mul", [uc, up], {})], {})]);
    fb.setInsertPoint(slow2);
    const cs = fb.emit("const", [], { kind: "number", value: slowConst });
    fb.br(j2, [fb.emit("mul", [cs, p], {})]);
    fb.sealBlock(j2);
    fb.setInsertPoint(j2);
    fb.ret(q);
    const fn = fb.finish();
    verifyFunction(fn);
    const stats = optimizeFunction(fn);
    verifyFunction(fn);
    return { fn: fn, stats: stats };
}

test("guard-merge: const +0 does not correspond to const -0 (attack H)", () => {
    // === would conflate the zeros; the rerouted slow path would flip
    // the sign of zero (1/x: Infinity vs -Infinity).  Must refuse.
    const { stats } = buildConstPairShape(0, -0);
    assert(stats.regions_merged === 0, `merge must be refused, got ${stats.regions_merged}`);
});

test("guard-merge: distinct NaN consts correspond (one JS NaN)", () => {
    // the flip side of Object.is: two const-NaN instructions denote the
    // same value on every path, so the honest twin merges
    const { stats } = buildConstPairShape(NaN, NaN);
    assert(stats.regions_merged === 1, `expected the merge, got ${stats.regions_merged}`);
});

test("guard-merge: the twin shape it refuses in attack F merges when honest", () => {
    // identical CFG to attack F but with the real generic twin
    // (slow: mul(p, p)) — the merge must fire.  Guards the twin check
    // against being accidentally over-strict.
    const fb = new FunctionBuilder("twin_ok", ["%env", "%this", "a"]);
    const a = fb.readVariable("a", fb.cur);
    const r1 = buildDiamond(fb, a, a, a, "r1");
    const r2 = buildDiamond(fb, r1.param, r1.param, r1.param, "r2");
    fb.ret(r2.param);
    const fn = fb.finish();
    verifyFunction(fn);
    const stats = optimizeFunction(fn);
    verifyFunction(fn);
    assert(stats.regions_merged === 1, `expected the merge, got ${stats.regions_merged}`);
});

test("rawJoin: a fully-proven loop-carried param converts to f64", () => {
    // loop header param fed box_f64 on BOTH the entry and back edges:
    // structurally qualified (f64-rooted), converts, and stays sound —
    // the dedicated test for the loop-carried conversion path.
    const fb = new FunctionBuilder("loopraw", ["%env", "%this", "a"]);
    const a = fb.readVariable("a", fb.cur);
    const ua = fb.emit("unbox_f64", [a], {});
    const ba = fb.emit("box_f64", [ua], {});
    const header = fb.newBlock("H");
    const hp = header.addParam("s");
    const body = fb.newBlock("body");
    const out = fb.newBlock("out");
    fb.br(header, [ba]);
    fb.setInsertPoint(header);
    const u = fb.emit("unbox_f64", [hp], {});
    const s = fb.emit("f64_add", [u, u], {});
    const bs = fb.emit("box_f64", [s], {});
    const lt = fb.emit("f64_lt", [s, s], {});
    fb.condBr(lt, body, [], out, []);
    fb.sealBlock(body);
    fb.setInsertPoint(body);
    fb.br(header, [bs]);
    fb.sealBlock(header);
    fb.sealBlock(out);
    fb.setInsertPoint(out);
    fb.ret(fb.emit("box_f64", [s], {}));
    const fn = fb.finish();
    verifyFunction(fn);
    const stats = optimizeFunction(fn);
    verifyFunction(fn);
    assert(stats.raw_join_params === 1, `raw_join_params = ${stats.raw_join_params}`);
    assert(hp.type === "f64" && hp.rawJoin, "loop param must be a marked f64 phi");
    assert(countOps(fn, "unbox_f64") === 1, "the loop-carried unbox collapses");
});

test("verifier: rawJoin marker admits f64 edge args into f64 params", () => {
    const fb = new FunctionBuilder("rawjoin", ["%env", "%this", "a"]);
    const a = fb.readVariable("a", fb.cur);
    const ua = fb.emit("unbox_f64", [a], {});
    const join = fb.newBlock("join");
    const jp = join.addParam("jp");
    jp.type = "f64";
    jp.rawJoin = true;
    fb.br(join, [ua]);
    fb.sealBlock(join);
    fb.setInsertPoint(join);
    fb.ret(fb.emit("box_f64", [jp], {}));
    verifyFunction(fb.finish()); // accepted
});

test("verifier: an f64 param without the rawJoin marker is rejected", () => {
    const fb = new FunctionBuilder("norawjoin", ["%env", "%this", "a"]);
    const a = fb.readVariable("a", fb.cur);
    const ua = fb.emit("unbox_f64", [a], {});
    const join = fb.newBlock("join");
    const jp = join.addParam("jp");
    jp.type = "f64"; // marker NOT set: the strict P2 rule stays in force
    fb.br(join, [ua]);
    fb.sealBlock(join);
    fb.setInsertPoint(join);
    fb.ret(fb.emit("box_f64", [jp], {}));
    // the edge-side strict rule fires first: without the marker the raw
    // f64 argument itself is rejected
    assertVerifyFails(fb.finish(), "must be boxed");
});

test("verifier: a boxed arg into a rawJoin f64 param is rejected", () => {
    const fb = new FunctionBuilder("boxedarg", ["%env", "%this", "a"]);
    const a = fb.readVariable("a", fb.cur);
    const join = fb.newBlock("join");
    const jp = join.addParam("jp");
    jp.type = "f64";
    jp.rawJoin = true;
    fb.br(join, [a]); // boxed value into the f64 param
    fb.sealBlock(join);
    fb.setInsertPoint(join);
    fb.ret(fb.emit("box_f64", [jp], {}));
    assertVerifyFails(fb.finish(), "f64 param");
});

// --- typed calling convention / function specialization ---------------

// mirror integrate.ts's ordering: lower, optimize, specialize, re-optimize
function specHarness(src: string, oracle: TypeOracle) {
    const analysis = new ScopeAnalysis();
    const info = analysis.analyzeFunction(parseFn(src));
    const module = new Module("m");
    const mod_ctx = {
        refs: new Map<string, never>(),
        oracle: oracle,
        typed_stats: { diamonds: 0, trusted: 0 },
    };
    const fn = lowerAnalyzedFunction(info, analysis, module, mod_ctx);
    verifyModule(module);
    optimizeModule(module);
    verifyModule(module);
    const stats = { specialized: 0, sites: 0, rejected: 0 };
    const changed = specializeModule(module, analysis, oracle, null, mod_ctx, stats);
    verifyModule(module);
    if (changed) {
        optimizeModule(module);
        verifyModule(module);
    }
    return { module: module, outer: fn, stats: stats };
}

// a loop keeps the callee out of the EIR inliner's single-block reach, so
// specialization (not inlining) must claim the call sites
const SPEC_KERNEL =
    "function k(n) { var s = 0; var i = 0; while (i < n) { s = s + i; i = i + 1; } return s; }";

test("specialize: local closed world clones and rewrites call sites", () => {
    const { module, outer, stats } = specHarness(
        `function outer() { ${SPEC_KERNEL} var r = k(10) + k(20); return r; }`,
        numericStubOracle(["n", "s", "i", "r"])
    );
    assert(stats.specialized === 1, `specialized=${stats.specialized}`);
    assert(stats.sites === 2, `sites=${stats.sites}`);
    assert(stats.rejected === 0, `rejected=${stats.rejected}`);
    const clone = module.functions.find((f) => f.name.indexOf("$typed") !== -1);
    assert(clone !== undefined, "clone emitted");
    assert(clone!.sig !== null && clone!.sig.result === "f64", "clone sig is f64-result");
    assert(clone!.sig!.formals.length === 1 && clone!.sig!.formals[0] === "f64", "f64 formal");
    // unguarded, slow-path-free body: no tag checks, no generic arithmetic
    assert(countOps(clone!, "has_tag") === 0, "clone carries no guards");
    assert(countOps(clone!, "add") === 0, "clone carries no generic ops");
    assert(countOps(clone!, "f64_add") >= 1, "clone computes raw");
    // every return is the raw f64 (printer shows the typed header)
    assertContains(printFunction(clone!), "): f64 {");
    // callers: both sites direct, generic dispatch and dead closure gone
    assert(countOps(outer, "call_typed") === 2, `call_typed=${countOps(outer, "call_typed")}`);
    assert(countOps(outer, "call") === 0, "no generic calls remain");
    assert(countOps(outer, "make_closure") === 0, "dead closure swept");
});

test("specialize: escaping closures are rejected even when the oracle lies", () => {
    // three escapes: as a return value, into an object literal, as a call
    // argument.  The (stub) oracle types everything {number} — a wrong
    // oracle must not widen what specializes; the STRUCTURAL escape
    // analysis rejects each one.
    for (const src of [
        `function outer() { ${SPEC_KERNEL} var r = k(1); return k; }`,
        // NB: the object must stay LIVE — a dead `{ m: k }` is sunk by the
        // optimizer before specialization runs, and an eliminated escape
        // is correctly no escape at all
        `function outer() { ${SPEC_KERNEL} var o = { m: k }; var r = k(1); return o; }`,
        `function outer(h) { ${SPEC_KERNEL} var r = h(k) + k(1); return r; }`,
    ]) {
        const { stats } = specHarness(src, numericStubOracle(["n", "s", "i", "r"]));
        assert(stats.specialized === 0, `specialized=${stats.specialized} for ${src}`);
        assert(stats.rejected === 0, `rejected=${stats.rejected} for ${src}`);
    }
});

test("specialize: env capture and `this` are structurally rejected post-lowering", () => {
    // k reads the enclosing c: its clone must load the env it was never
    // given — discarded by the envParam post-check, not by the oracle
    const cap = specHarness(
        `function outer(c) { function k(n) { var s = 0; while (s < n) { s = s + c; } return s; } var r = k(10); return r; }`,
        numericStubOracle(["n", "s", "c", "r"])
    );
    assert(cap.stats.specialized === 0, `specialized=${cap.stats.specialized}`);
    assert(cap.stats.rejected === 1, `rejected=${cap.stats.rejected}`);
    // `this` use survives the (lying) type gate; the thisParam post-check
    // discards the clone
    const ths = specHarness(
        `function outer() { function k(n) { var s = this.z; while (s < n) { s = s + 1; } return s; } var r = k(10); return r; }`,
        numericStubOracle(["n", "s", "r"])
    );
    assert(ths.stats.specialized === 0, `specialized=${ths.stats.specialized}`);
    assert(ths.stats.rejected === 1, `rejected=${ths.stats.rejected}`);
});

test("specialize: non-numeric profiles and non-value returns disqualify early", () => {
    for (const [src, names] of [
        // a bare `return;` — no f64 result to promise
        [
            `function outer() { function k(n) { var s = 0; while (s < n) { s = s + 1; } if (s < 0) return; return s; } var r = k(5); return r; }`,
            ["n", "s", "r"],
        ],
        // params not provably {number}
        [
            `function outer() { ${SPEC_KERNEL} var r = k(10); return r; }`,
            ["s", "i", "r"], // n missing: top
        ],
        // arguments-object use
        [
            `function outer() { function k(n) { var s = arguments.length; while (s < n) { s = s + 1; } return s; } var r = k(5); return r; }`,
            ["n", "s", "r"],
        ],
    ] as [string, string[]][]) {
        const { stats } = specHarness(src, numericStubOracle(names));
        assert(stats.specialized === 0, `specialized=${stats.specialized} for ${src}`);
    }
});

test("specialize: arity-mismatched sites keep the generic path", () => {
    // k(10, 99) passes an extra arg: still an enumerated site (correct to
    // leave generic), so the clone ships and only the exact-arity site
    // rewrites — the closure must SURVIVE for the generic site
    const { module, outer, stats } = specHarness(
        `function outer() { ${SPEC_KERNEL} var r = k(10) + k(20, 99); return r; }`,
        numericStubOracle(["n", "s", "i", "r"])
    );
    assert(stats.specialized === 1, `specialized=${stats.specialized}`);
    assert(stats.sites === 1, `sites=${stats.sites}`);
    assert(countOps(outer, "call_typed") === 1, "one direct site");
    assert(countOps(outer, "call") === 1, "one generic site survives");
    assert(countOps(outer, "make_closure") === 1, "closure still needed");
    assert(module.functions.some((f) => f.sig !== null), "clone present");
});

test("verifier: an f64 entry param requires a matching sig", () => {
    const fb = new FunctionBuilder("sigless", ["%env", "%this", "x"]);
    const x = fb.fn.entry!.params[2]!;
    x.type = "f64";
    fb.ret(fb.constUndefined());
    assertVerifyFails(fb.finish(), "rawJoin");

    const fb2 = new FunctionBuilder("sigged", ["%env", "%this", "x"]);
    fb2.fn.sig = { formals: ["f64"], result: "any" };
    const x2 = fb2.fn.entry!.params[2]!;
    x2.type = "f64";
    fb2.ret(fb2.emit("box_f64", [x2], {}));
    verifyFunction(fb2.finish());
});

test("verifier: an f64-result function must return raw f64", () => {
    const fb = new FunctionBuilder("f64ret", ["%env", "%this", "x"]);
    fb.fn.sig = { formals: ["f64"], result: "f64" };
    fb.fn.entry!.params[2]!.type = "f64";
    fb.ret(fb.constUndefined());
    assertVerifyFails(fb.finish(), "f64-result");
});

test("verifier: call_typed is checked against the callee sig", () => {
    const mkCallee = (): Func => {
        const fb = new FunctionBuilder("callee$typed", ["%env", "%this", "x"]);
        fb.fn.sig = { formals: ["f64"], result: "f64" };
        const x = fb.fn.entry!.params[2]!;
        x.type = "f64";
        fb.ret(fb.emit("f64_add", [x, x], {}));
        return fb.finish();
    };
    const mkCaller = (argIsRaw: boolean, resultType: string, calleeName: string): Func => {
        const fb = new FunctionBuilder("caller", ["%env", "%this", "a"]);
        const a = fb.readVariable("a", fb.cur);
        const env = fb.constUndefined();
        const arg = argIsRaw ? fb.emit("unbox_f64", [a], {}) : a;
        const ct = fb.emit("call_typed", [env, arg], { fn: calleeName });
        ct.type = resultType;
        fb.ret(fb.emit("box_f64", [ct], {}));
        return fb.finish();
    };
    const assertModuleFails = (m: Module, needle: string): void => {
        try {
            verifyModule(m);
        } catch (err) {
            const msg = (err as Error).message;
            if (msg.indexOf(needle) === -1)
                throw new Error(`verifier failed, but with '${msg}' (wanted '${needle}')`);
            return;
        }
        throw new Error(`verifier accepted a bad call_typed (wanted '${needle}')`);
    };

    // well-typed: passes
    const ok = new Module("ok");
    ok.addFunction(mkCallee());
    ok.addFunction(mkCaller(true, "f64", "callee$typed"));
    verifyModule(ok);

    // boxed arg into an f64 formal
    const bad1 = new Module("bad1");
    bad1.addFunction(mkCallee());
    bad1.addFunction(mkCaller(false, "f64", "callee$typed"));
    assertModuleFails(bad1, "wants f64");

    // stamped result type contradicts the callee sig
    const bad2 = new Module("bad2");
    bad2.addFunction(mkCallee());
    const wrongResult = (() => {
        const fb = new FunctionBuilder("caller2", ["%env", "%this", "a"]);
        const a = fb.readVariable("a", fb.cur);
        const arg = fb.emit("unbox_f64", [a], {});
        const ct = fb.emit("call_typed", [fb.constUndefined(), arg], { fn: "callee$typed" });
        // ct.type left "any": lies about the f64 result
        fb.ret(ct);
        return fb.finish();
    })();
    bad2.addFunction(wrongResult);
    assertModuleFails(bad2, "result type");

    // unknown callee
    const bad3 = new Module("bad3");
    bad3.addFunction(mkCaller(true, "f64", "nowhere$typed"));
    assertModuleFails(bad3, "unknown function");
});

test("opt: unbox_f64 of a number const folds to f64_const", () => {
    const fb = new FunctionBuilder("cfold", ["%env", "%this"]);
    const c = fb.constNumber(2);
    const u = fb.emit("unbox_f64", [c], {});
    const v = fb.emit("f64_add", [u, u], {});
    fb.ret(fb.emit("box_f64", [v], {}));
    const fn = fb.finish();
    verifyFunction(fn);
    const s = optimizeFunction(fn);
    verifyFunction(fn);
    assert(s.unbox_folds === 1, `unbox_folds=${s.unbox_folds}`);
    assert(countOps(fn, "f64_const") === 1, "raw const minted");
    assert(countOps(fn, "unbox_f64") === 0, "unbox gone");
});

test("opt: constant boolean edges thread past to_boolean re-tests", () => {
    // the `<` diamond's fast arm: after threading, its constant edges
    // branch directly and only the slow (generic) edge still re-tests
    const r = lowerFunctionNode(
        parseFn("function f(x, y) { if (x < y) { return 1; } return 2; }"),
        undefined,
        numericStubOracle(["x", "y"])
    );
    verifyModule(r.module);
    const s = optimizeFunction(r.fn, r.module);
    verifyFunction(r.fn);
    assert(s.joins_threaded === 2, `joins_threaded=${s.joins_threaded}`);
    // the join survives for the slow arm's boxed value, still re-tested
    assert(countOps(r.fn, "to_boolean") === 1, "slow-arm re-test survives");
});

// --- oracle: TypeSig -> EirType mapping -----------------------------------------

test("oracle: TypeSig constituents map to EirType tags", () => {
    const t = typeSigToEirType("num|str");
    assert(t.tags !== "top");
    const tags = t.tags as ReadonlySet<string>;
    assert(tags.size === 2 && tags.has("number") && tags.has("string"));
    const all = typeSigToEirType("num|str|bool|null|undefined|fn|obj").tags as ReadonlySet<string>;
    assert(all.size === 7 && all.has("closure") && all.has("object") && all.has("null"));
});

test("oracle: top, never, and missing sigs are all top", () => {
    assert(typeSigToEirType("\u22a4").tags === "top");
    assert(typeSigToEirType("never").tags === "top");
    assert(typeSigToEirType(undefined).tags === "top");
});

test("oracle: an unrecognized constituent is top, never a guess", () => {
    assert(typeSigToEirType("num|widget").tags === "top");
    assert(typeSigToEirType("bigint").tags === "top");
    assert(typeSigToEirType("").tags === "top");
});

// --- shape-guarded property access ----------------------------

test("shape-oracle: TypeSig -> repr (num=f64, non-num unions=boxed, straddles decline)", () => {
    assert(typeSigToShapeRepr("num") === "f64");
    assert(typeSigToShapeRepr("str") === "boxed");
    assert(typeSigToShapeRepr("str|bool|undefined|null|obj|fn") === "boxed");
    assert(typeSigToShapeRepr("num|str") === null);
    assert(typeSigToShapeRepr("⊤") === null);
    assert(typeSigToShapeRepr("never") === null);
    assert(typeSigToShapeRepr("num|widget") === null);
});

// a stub oracle with receiver-shape facts: types Identifier receivers by
// name; everything else declines as unmapped (the real oracle's fail-soft).
// A receiver may carry one shape (mono) or two (the poly chain).
function stubShapeOracle(
    shapes: Record<string, OracleShapeField[] | OracleShapeField[][] | undefined>,
    types?: Record<string, TypeTag[] | undefined>
): TypeOracle {
    const base = stubOracle(types || {});
    return {
        ...base,
        receiverShapeOfNode: (n) => {
            const id = n as { type?: string; name?: string };
            const entry =
                id.type === "Identifier" && id.name !== undefined ? shapes[id.name] : undefined;
            if (!entry) return { declined: "unmapped" };
            const list = Array.isArray(entry[0]) ? (entry as OracleShapeField[][]) : [entry as OracleShapeField[]];
            return { shapes: list };
        },
    };
}

const PXY: OracleShapeField[] = [
    { name: "x", repr: "f64" },
    { name: "y", repr: "f64" },
    { name: "s", repr: "boxed" },
];

test("shapes: exact receiver fact lowers a get to the has_shape diamond", () => {
    const { printed } = lowerWithOracle(
        "function f(p) { return p.y; }",
        stubShapeOracle({ p: PXY })
    );
    assertContains(printed, 'has_shape');
    assertContains(printed, 'shape="x:f64,y:f64,s:boxed"');
    assertContains(printed, 'slot_load');
    assertContains(printed, 'slot=1');
    assertContains(printed, 'repr="f64"');
    assertContains(printed, 'get_prop_atom'); // the slow arm survives
    assertContains(printed, "shape_join");
});

test("shapes: no shape query support means today's lowering exactly", () => {
    const { printed } = lowerWithOracle(
        "function f(p) { return p.y; }",
        stubOracle({ p: undefined })
    );
    assertNotContains(printed, "has_shape");
    assertNotContains(printed, "slot_load");
});

test("shapes: a field outside the shape declines (proto/method access)", () => {
    const { printed } = lowerWithOracle(
        "function f(p) { return p.z; }",
        stubShapeOracle({ p: PXY })
    );
    assertNotContains(printed, "has_shape");
    assertContains(printed, 'get_prop_atom');
});

test("shapes: an f64-field store guards has_shape AND has_tag, numbers fast", () => {
    const { printed } = lowerWithOracle(
        "function f(p, v) { p.x = v; }",
        stubShapeOracle({ p: PXY })
    );
    assertContains(printed, "has_shape");
    assertContains(printed, "has_tag");
    assertContains(printed, "slot_store");
    assertContains(printed, "set_prop_atom");
    // f64 field: the tag-true edge is the fast arm
    assert(
        /cond_br %\d+ -> \^shape_setfast\d+\(\), \^shape_setslow\d+\(\)/.test(printed),
        "expected tag-true -> fast for an f64 field"
    );
});

test("shapes: a boxed-field store takes non-numbers fast (swapped tag arms)", () => {
    const { printed } = lowerWithOracle(
        "function f(p, v) { p.s = v; }",
        stubShapeOracle({ p: PXY })
    );
    assertContains(printed, 'repr="boxed"');
    // boxed field: the tag-true edge is the SLOW arm
    assert(
        /cond_br %\d+ -> \^shape_setslow\d+\(\), \^shape_setfast\d+\(\)/.test(printed),
        "expected tag-true -> slow for a boxed field"
    );
});

test("shapes: EJS_NO_SHAPE_GUARDS disables the diamonds", () => {
    process.env["EJS_NO_SHAPE_GUARDS"] = "1";
    try {
        const { printed } = lowerWithOracle(
            "function f(p) { return p.y; }",
            stubShapeOracle({ p: PXY })
        );
        assertNotContains(printed, "has_shape");
    } finally {
        delete process.env["EJS_NO_SHAPE_GUARDS"];
    }
});

// --- 2-way polymorphic guard chains ----------------------------

// the second class of the poly pair: same fields x/y at DIFFERENT slots
// (plus its own z), so per-arm slot immediates are observable
const PZXY: OracleShapeField[] = [
    { name: "z", repr: "f64" },
    { name: "x", repr: "f64" },
    { name: "y", repr: "f64" },
];

test("shapes-poly: two exact shapes lower a get to a guard chain, one slow path", () => {
    const { printed } = lowerWithOracle(
        "function f(p) { return p.y; }",
        stubShapeOracle({ p: [PXY, PZXY] })
    );
    const guards = (printed.match(/has_shape/g) || []).length;
    assert(guards === 2, `expected 2 chained guards, got ${guards}`);
    assertContains(printed, 'shape="x:f64,y:f64,s:boxed"');
    assertContains(printed, 'shape="z:f64,x:f64,y:f64"');
    assertContains(printed, "shape_chk"); // the second guard tests on the first's miss edge
    assertContains(printed, "slot=1"); // y in {x,y,s}
    assertContains(printed, "slot=2"); // y in {z,x,y}
    const slows = (printed.match(/get_prop_atom/g) || []).length;
    assert(slows === 1, `the chain shares ONE generic slow path, got ${slows}`);
});

test("shapes-poly: a field absent from either shape declines the whole site", () => {
    // s lives only in PXY: the PZXY arm would need proto-lookup semantics,
    // which only the generic path has (criterion 2 — no near-misses)
    const { printed } = lowerWithOracle(
        "function f(p) { return p.s; }",
        stubShapeOracle({ p: [PXY, PZXY] })
    );
    assertNotContains(printed, "has_shape");
    assertContains(printed, "get_prop_atom");
});

test("shapes-poly: stores chain with a tag split per arm, one generic path", () => {
    const { printed } = lowerWithOracle(
        "function f(p, v) { p.x = v; }",
        stubShapeOracle({ p: [PXY, PZXY] })
    );
    assert((printed.match(/has_shape/g) || []).length === 2, "2 chained guards");
    assert((printed.match(/has_tag/g) || []).length === 2, "a tag split per arm");
    assert((printed.match(/slot_store/g) || []).length === 2, "a typed store per arm");
    assert((printed.match(/set_prop_atom/g) || []).length === 1, "one generic path");
    assertContains(printed, "shape_setchk");
});

test("shapes-poly: mixed reprs orient each arm by its own field repr", () => {
    const A: OracleShapeField[] = [{ name: "x", repr: "f64" }];
    const B: OracleShapeField[] = [
        { name: "x", repr: "boxed" },
        { name: "w", repr: "boxed" },
    ];
    const get = lowerWithOracle(
        "function f(p) { return p.x; }",
        stubShapeOracle({ p: [A, B] })
    ).printed;
    // only the f64 arm boxes its raw load
    assert((get.match(/box_f64/g) || []).length === 1, "exactly one arm boxes");
    assertContains(get, 'repr="f64"');
    assertContains(get, 'repr="boxed"');
    const set = lowerWithOracle(
        "function f(p, v) { p.x = v; }",
        stubShapeOracle({ p: [A, B] })
    ).printed;
    // one arm takes numbers fast (tag-true -> fast), the other non-numbers
    assert(
        /cond_br %\d+ -> \^shape_setfast\d+\(\), \^shape_setslow\d+\(\)/.test(set),
        "f64 arm: tag-true -> fast"
    );
    assert(
        /cond_br %\d+ -> \^shape_setslow\d+\(\), \^shape_setfast\d+\(\)/.test(set),
        "boxed arm: tag-true -> slow"
    );
});

test("shapes-poly: EJS_NO_POLY_SHAPE_GUARDS declines 2-shape sites, keeps mono", () => {
    process.env["EJS_NO_POLY_SHAPE_GUARDS"] = "1";
    try {
        const poly = lowerWithOracle(
            "function f(p) { return p.y; }",
            stubShapeOracle({ p: [PXY, PZXY] })
        ).printed;
        assertNotContains(poly, "has_shape");
        const mono = lowerWithOracle(
            "function f(p) { return p.y; }",
            stubShapeOracle({ p: PXY })
        ).printed;
        assertContains(mono, "has_shape");
    } finally {
        delete process.env["EJS_NO_POLY_SHAPE_GUARDS"];
    }
});

test("shapes-poly: structurally equal shapes reported twice guard once", () => {
    const { printed } = lowerWithOracle(
        "function f(p) { return p.y; }",
        stubShapeOracle({ p: [PXY, PXY] })
    );
    const guards = (printed.match(/has_shape/g) || []).length;
    assert(guards === 1, `duplicate shapes must dedupe to a mono diamond, got ${guards}`);
});

// --- shapes: verifier rules (hand-built attack IR) -------------------------------

function assertThrows(fn: () => void, needle: string): void {
    let threw: string | null = null;
    try {
        fn();
    } catch (e) {
        threw = (e as Error).message;
    }
    assert(threw !== null, `expected a verifier rejection containing '${needle}'`);
    assert(
        threw!.includes(needle),
        `expected rejection containing '${needle}', got: ${threw}`
    );
}

interface SlotAttackOpts {
    guarded?: boolean; // guard the slot op with has_shape (default true)
    killInFast?: boolean; // a call between the guard and the slot op
    store?: boolean; // slot_store instead of slot_load
    storeRaw?: boolean; // unbox the stored value (the typed store form)
    tagGuard?: "none" | "true" | "false"; // has_tag fact for the stored value
    slot?: number;
    repr?: string;
    boxedField?: boolean; // shape's x field is boxed (for boxed-store rules)
    loadType?: string; // override the slot_load result stamp (attack)
    shapeImm?: string; // override the op's shape imm
}

// head: guard (or an unrelated to_boolean test) -> fast/slow -> join
function buildSlotAttack(o: SlotAttackOpts): { mod: Module; fn: Func } {
    const guarded = o.guarded !== false;
    const fb = new FunctionBuilder("attack", ["%env", "%this", "p", "v"]);
    const p = fb.fn.entry!.params[2]!;
    const v = fb.fn.entry!.params[3]!;
    const shapeKey = o.boxedField ? "x:boxed,y:f64" : "x:f64,y:f64";
    const opShape = o.shapeImm ?? shapeKey;
    const repr = o.repr ?? (o.boxedField ? "boxed" : "f64");

    const fast = fb.newBlock("fast");
    const slow = fb.newBlock("slow");
    const join = fb.newBlock("join");
    const res = join.addParam("res");
    const cond = guarded
        ? fb.emit("has_shape", [p], { shape: shapeKey })
        : fb.emit("to_boolean", [p], {});
    fb.condBr(cond, fast, [], slow, []);
    fb.sealBlock(fast);
    fb.sealBlock(slow);

    fb.setInsertPoint(fast);
    if (o.killInFast) fb.emit("call_runtime", [], { name: "ToString" });
    let stored = v;
    if (o.store && o.tagGuard && o.tagGuard !== "none") {
        // establish the tag fact: a nested has_tag diamond whose surviving
        // arm continues to the store
        const tagok = fb.newBlock("tagok");
        const tagbail = fb.newBlock("tagbail");
        const t = fb.emit("has_tag", [stored], { tag: "number" });
        if (o.tagGuard === "true") fb.condBr(t, tagok, [], tagbail, []);
        else fb.condBr(t, tagbail, [], tagok, []);
        fb.sealBlock(tagok);
        fb.sealBlock(tagbail);
        fb.setInsertPoint(tagbail);
        fb.br(join, [fb.constUndefined()]);
        fb.setInsertPoint(tagok);
    }
    if (o.storeRaw) stored = fb.emit("unbox_f64", [stored], {});
    let fastv: Inst;
    if (o.store) {
        fastv = fb.emit("slot_store", [p, stored], {
            shape: opShape,
            slot: o.slot ?? 0,
            repr: repr,
        });
        fb.br(join, [fb.constUndefined()]);
    } else {
        fastv = fb.emit("slot_load", [p], {
            shape: opShape,
            slot: o.slot ?? 0,
            repr: repr,
        });
        // an f64-repr load produces a raw f64 (stamped by lowering)
        // and boxes at the fast exit; loadType overrides for attack IR
        fastv.type = o.loadType ?? (repr === "f64" ? "f64" : "any");
        if (fastv.type === "f64") fastv = fb.emit("box_f64", [fastv], {});
        fb.br(join, [fastv]);
    }

    fb.setInsertPoint(slow);
    const g = fb.emit("get_prop_atom", [p], { atom: "x" });
    fb.br(join, [g]);

    fb.sealBlock(join);
    fb.setInsertPoint(join);
    fb.ret(res);

    const fn = fb.finish();
    const mod = new Module("attack_mod");
    mod.addFunction(fn);
    mod.internShape([
        { name: "x", repr: o.boxedField ? "boxed" : "f64" },
        { name: "y", repr: "f64" },
    ]);
    return { mod, fn };
}

test("shapes-verify: a guarded slot_load in the guard's true arm verifies", () => {
    const { mod } = buildSlotAttack({});
    verifyModule(mod);
});

test("shapes-verify: a slot op without a has_shape fact is rejected", () => {
    const { mod } = buildSlotAttack({ guarded: false });
    assertThrows(() => verifyModule(mod), "un-killed has_shape fact");
});

test("shapes-verify: a WRITE|CALL between guard and slot op kills the fact", () => {
    const { mod } = buildSlotAttack({ killInFast: true });
    assertThrows(() => verifyModule(mod), "un-killed has_shape fact");
});

test("shapes-verify: slot out of bounds / repr mismatch / unknown shape reject", () => {
    assertThrows(() => verifyModule(buildSlotAttack({ slot: 2 }).mod), "out of bounds");
    assertThrows(() => verifyModule(buildSlotAttack({ repr: "boxed" }).mod), "shape field repr");
    assertThrows(
        () => verifyModule(buildSlotAttack({ shapeImm: "a:boxed" }).mod),
        "unknown module shape"
    );
});

test("shapes-verify: slot_store repr proofs — typed f64, tagged boxed", () => {
    // an f64 store takes a raw f64 — the type system IS the proof;
    // no has_tag fact anywhere and it still verifies
    verifyModule(buildSlotAttack({ store: true, storeRaw: true }).mod);
    // a BOXED value into an f64 slot is a type error, has_tag fact or not
    assertThrows(
        () => verifyModule(buildSlotAttack({ store: true }).mod),
        "raw f64"
    );
    assertThrows(
        () => verifyModule(buildSlotAttack({ store: true, tagGuard: "true" }).mod),
        "raw f64"
    );
    // a boxed-repr store still requires the has_tag=false fact
    verifyModule(buildSlotAttack({ store: true, boxedField: true, tagGuard: "false" }).mod);
    assertThrows(
        () => verifyModule(buildSlotAttack({ store: true, boxedField: true, tagGuard: "none" }).mod),
        "has_tag"
    );
    // the WRONG edge's fact (value proven number, field repr boxed) rejects
    assertThrows(
        () => verifyModule(buildSlotAttack({ store: true, boxedField: true, tagGuard: "true" }).mod),
        "has_tag"
    );
    // a raw f64 into a BOXED slot is a type error
    assertThrows(
        () =>
            verifyModule(
                buildSlotAttack({ store: true, boxedField: true, storeRaw: true }).mod
            ),
        "boxed value"
    );
});

test("shapes-verify: slot_load result stamp must match its repr", () => {
    // an f64-repr load left stamped "any" is rejected (the boxed
    // form no longer verifies)...
    assertThrows(
        () => verifyModule(buildSlotAttack({ loadType: "any" }).mod),
        "must have type f64"
    );
    // ...and a boxed-repr load stamped f64 likewise
    assertThrows(
        () => verifyModule(buildSlotAttack({ boxedField: true, loadType: "f64" }).mod),
        "must have type any"
    );
});

// --- shapes: optimizer (merging + fact folding) ----------------------------------

function shapeOptStats(): OptStats {
    return {
        allocs_sunk: 0,
        reads_folded: 0,
        calls_inlined: 0,
        iters_folded: 0,
        dead_removed: 0,
        guards_folded: 0,
        regions_merged: 0,
        raw_join_params: 0,
        shape_guards_folded: 0,
        shape_regions_merged: 0,
        shape_numeric_merged: 0,
        unbox_folds: 0,
        joins_threaded: 0,
        shape_allocs_sunk: 0,
        shape_guards_sunk: 0,
        args_sunk: 0,
        flow_allocs_sunk: 0,
        allocs_materialized: 0,
    };
}

function lowerShapeOpt(src: string): { printed: string; stats: OptStats } {
    const r = lowerFunctionNode(parseFn(src), undefined, stubShapeOracle({ p: PXY }));
    verifyModule(r.module);
    const stats = optimizeFunction(r.fn, r.module);
    verifyModule(r.module);
    return { printed: printFunction(r.fn), stats };
}

test("shapes-opt: consecutive gets on one receiver merge to one guard region", () => {
    const { printed, stats } = lowerShapeOpt("function f(p) { return p.x + p.x; }");
    assert(stats.shape_regions_merged === 1, `merged=${stats.shape_regions_merged}`);
    assert(stats.shape_guards_folded === 1, `folded=${stats.shape_guards_folded}`);
    const guards = (printed.match(/has_shape/g) || []).length;
    assert(guards === 1, `expected 1 surviving has_shape, got ${guards}`);
    const loads = (printed.match(/slot_load/g) || []).length;
    assert(loads === 2, `expected 2 slot_loads, got ${loads}`);
});

test("shapes-poly-opt: chains pass the optimizer un-merged and re-verify", () => {
    // The region matcher and fact folder are mono-strict by construction:
    // a poly chain's first guard has the second CHECK block as its miss
    // edge (not a generic slow arm) and its join has three predecessors,
    // so both machineries must refuse — everything survives verbatim and
    // the module re-verifies.  (Chain-aware merging is future measured
    // work; kernel wall time is at mono parity without it.)
    const r = lowerFunctionNode(
        parseFn("function f(p) { return p.x + p.x; }"),
        undefined,
        stubShapeOracle({ p: [PXY, PZXY] })
    );
    verifyModule(r.module);
    const stats = optimizeFunction(r.fn, r.module);
    verifyModule(r.module);
    const printed = printFunction(r.fn);
    assert(stats.shape_regions_merged === 0, `merged=${stats.shape_regions_merged}`);
    assert(stats.shape_guards_folded === 0, `folded=${stats.shape_guards_folded}`);
    assert(stats.shape_numeric_merged === 0, `het-merged=${stats.shape_numeric_merged}`);
    const guards = (printed.match(/has_shape/g) || []).length;
    assert(guards === 4, `2 sites x 2 chained guards must survive, got ${guards}`);
});

test("shapes-opt: a call between accesses kills the facts and refuses the merge", () => {
    const { printed, stats } = lowerShapeOpt(
        "function f(p, g) { var a = p.x; g(); return a + p.x; }"
    );
    assert(stats.shape_regions_merged === 0, `merged=${stats.shape_regions_merged}`);
    assert(stats.shape_guards_folded === 0, `folded=${stats.shape_guards_folded}`);
    const guards = (printed.match(/has_shape/g) || []).length;
    assert(guards === 2, `expected both has_shape guards to survive, got ${guards}`);
});

test("shapes-opt: store diamonds do not match the get-region shape", () => {
    const { stats } = lowerShapeOpt("function f(p) { p.x = p.x + 1; return p.x; }");
    // the has_tag split in the store's fast side refuses region matching;
    // nothing may merge across a slot_store (it is a WRITE kill)
    assert(stats.shape_regions_merged === 0, `merged=${stats.shape_regions_merged}`);
});

// hand-built twin-mismatch attack: two adjacent get regions whose slow
// arms LIE (region2's generic get names a different field than its fast
// slot_load) — the merge must refuse on the twin check
function buildTwinAttack(lieAtom: string): { mod: Module; fn: Func; stats: OptStats } {
    const fb = new FunctionBuilder("twin", ["%env", "%this", "p"]);
    const p = fb.fn.entry!.params[2]!;
    const shapeKey = "x:f64,y:f64";

    const fast1 = fb.newBlock("fast1");
    const slow1 = fb.newBlock("slow1");
    const j1 = fb.newBlock("j1");
    const p1 = j1.addParam("v1");
    const g1 = fb.emit("has_shape", [p], { shape: shapeKey });
    fb.condBr(g1, fast1, [], slow1, []);
    fb.sealBlock(fast1);
    fb.sealBlock(slow1);
    fb.setInsertPoint(fast1);
    const l1 = fb.emit("slot_load", [p], { shape: shapeKey, slot: 0, repr: "f64" });
    l1.type = "f64";
    fb.br(j1, [fb.emit("box_f64", [l1], {})]);
    fb.setInsertPoint(slow1);
    const gp1 = fb.emit("get_prop_atom", [p], { atom: "x" });
    fb.br(j1, [gp1]);
    fb.sealBlock(j1);
    fb.setInsertPoint(j1);

    const fast2 = fb.newBlock("fast2");
    const slow2 = fb.newBlock("slow2");
    const j2 = fb.newBlock("j2");
    const p2 = j2.addParam("v2");
    const g2 = fb.emit("has_shape", [p], { shape: shapeKey });
    fb.condBr(g2, fast2, [], slow2, []);
    fb.sealBlock(fast2);
    fb.sealBlock(slow2);
    fb.setInsertPoint(fast2);
    const l2 = fb.emit("slot_load", [p], { shape: shapeKey, slot: 0, repr: "f64" });
    l2.type = "f64";
    fb.br(j2, [fb.emit("box_f64", [l2], {})]);
    fb.setInsertPoint(slow2);
    const gp2 = fb.emit("get_prop_atom", [p], { atom: lieAtom });
    fb.br(j2, [gp2]);
    fb.sealBlock(j2);
    fb.setInsertPoint(j2);
    const sum = fb.emit("add", [p1, p2], {});
    fb.ret(sum);

    const fn = fb.finish();
    const mod = new Module("twin_mod");
    mod.addFunction(fn);
    mod.internShape([
        { name: "x", repr: "f64" },
        { name: "y", repr: "f64" },
    ]);
    const stats = shapeOptStats();
    optimizeShapeRegions(fn, mod, stats);
    verifyModule(mod);
    return { mod, fn, stats };
}

test("shapes-opt: a lying slow twin refuses the merge; the honest one merges", () => {
    const lying = buildTwinAttack("y");
    assert(lying.stats.shape_regions_merged === 0, "lying twin must not merge");
    const honest = buildTwinAttack("x");
    assert(honest.stats.shape_regions_merged === 1, "honest twin must merge");
    assert(honest.stats.shape_guards_folded === 1, "post-merge guard must fold");
});

// stale-compare attack: the fact holds at the branch, but the compare was
// computed BEFORE the region that establishes it — folding it to true
// would take the wrong arm when the compare was false at its own site
test("shapes-opt: a stale (earlier-block) has_shape compare never folds", () => {
    const fb = new FunctionBuilder("stale", ["%env", "%this", "p"]);
    const p = fb.fn.entry!.params[2]!;
    const shapeKey = "x:f64,y:f64";
    const t1 = fb.newBlock("t1");
    const out = fb.newBlock("out");
    const a = fb.newBlock("a");
    const bb = fb.newBlock("b");
    const stale = fb.emit("has_shape", [p], { shape: shapeKey });
    const g1 = fb.emit("has_shape", [p], { shape: shapeKey });
    fb.condBr(g1, t1, [], out, []);
    fb.sealBlock(t1);
    fb.setInsertPoint(t1);
    fb.condBr(stale, a, [], bb, []);
    fb.sealBlock(a);
    fb.sealBlock(bb);
    fb.setInsertPoint(a);
    fb.br(out, []);
    fb.setInsertPoint(bb);
    fb.br(out, []);
    fb.sealBlock(out);
    fb.setInsertPoint(out);
    fb.ret(fb.constUndefined());

    const fn = fb.finish();
    const mod = new Module("stale_mod");
    mod.addFunction(fn);
    mod.internShape([
        { name: "x", repr: "f64" },
        { name: "y", repr: "f64" },
    ]);
    verifyModule(mod);
    const stats = shapeOptStats();
    optimizeShapeRegions(fn, mod, stats);
    assert(stats.shape_guards_folded === 0, "stale compare must not fold");

    // the same CFG with the compare minted fresh in t1 DOES fold
    const fb2 = new FunctionBuilder("fresh", ["%env", "%this", "p"]);
    const q = fb2.fn.entry!.params[2]!;
    const t1b = fb2.newBlock("t1");
    const outb = fb2.newBlock("out");
    const ab = fb2.newBlock("a");
    const bbb = fb2.newBlock("b");
    const g = fb2.emit("has_shape", [q], { shape: shapeKey });
    fb2.condBr(g, t1b, [], outb, []);
    fb2.sealBlock(t1b);
    fb2.setInsertPoint(t1b);
    const fresh = fb2.emit("has_shape", [q], { shape: shapeKey });
    fb2.condBr(fresh, ab, [], bbb, []);
    fb2.sealBlock(ab);
    fb2.sealBlock(bbb);
    fb2.setInsertPoint(ab);
    fb2.br(outb, []);
    fb2.setInsertPoint(bbb);
    fb2.br(outb, []);
    fb2.sealBlock(outb);
    fb2.setInsertPoint(outb);
    fb2.ret(fb2.constUndefined());
    const fn2 = fb2.finish();
    const mod2 = new Module("fresh_mod");
    mod2.addFunction(fn2);
    mod2.internShape([
        { name: "x", repr: "f64" },
        { name: "y", repr: "f64" },
    ]);
    const stats2 = shapeOptStats();
    optimizeShapeRegions(fn2, mod2, stats2);
    assert(stats2.shape_guards_folded === 1, "fresh dominated compare must fold");
    verifyModule(mod2);
});

// --- typed slots + heterogeneous fusion ------------------------

test("shapes-typed: f64 loads are raw + boxed at the exit; stores unbox", () => {
    const g = lowerWithOracle("function f(p) { return p.x; }", stubShapeOracle({ p: PXY }));
    assertContains(g.printed, ": f64 = slot_load");
    assertContains(g.printed, "box_f64");
    const s = lowerWithOracle("function f(p, v) { p.x = v; }", stubShapeOracle({ p: PXY }));
    assertContains(s.printed, "unbox_f64");
    // boxed fields keep boxed access — no raw traffic anywhere
    const b = lowerWithOracle("function f(p) { return p.s; }", stubShapeOracle({ p: PXY }));
    assertNotContains(b.printed, "box_f64");
    assertNotContains(b.printed, ": f64 = slot_load");
});

test("shapes-typed: shape and numeric regions fuse unboxed end-to-end", () => {
    // the real oracle types f64-field member reads as {number}, which is
    // what makes lowering wrap the arithmetic in numeric diamonds — the
    // stub must too, or there is no numeric region to fuse
    const oracle: TypeOracle = {
        ...stubShapeOracle({ p: PXY }),
        typeOfNode: (n) => {
            const t = (n as { type?: string }).type;
            return t === "MemberExpression" ? { tags: new Set(["number"]) } : { tags: "top" };
        },
    };
    const r = lowerFunctionNode(
        parseFn("function f(p) { return p.x * p.x + p.y * p.y; }"),
        undefined,
        oracle
    );
    verifyModule(r.module);
    const stats = optimizeFunction(r.fn, r.module);
    verifyModule(r.module);
    const printed = printFunction(r.fn);
    assert(stats.shape_numeric_merged >= 2, `het merges=${stats.shape_numeric_merged}`);
    assert(stats.shape_regions_merged >= 2, `shape merges=${stats.shape_regions_merged}`);
    const guards = (printed.match(/has_shape/g) || []).length;
    assert(guards === 1, `expected 1 surviving has_shape, got ${guards}`);
    const tags = (printed.match(/has_tag/g) || []).length;
    assert(tags === 0, `expected every has_tag folded, got ${tags}`);
    const rawLoads = (printed.match(/: f64 = slot_load/g) || []).length;
    assert(rawLoads === 4, `expected 4 raw slot_loads, got ${rawLoads}`);
    assert(stats.raw_join_params >= 2, `raw join params=${stats.raw_join_params}`);
});

// re-execution attack: region1's slow chain holds a generic mul fed by a
// get of a BOXED-repr field — re-running it after the fast side is not
// provably pure, so any merge below must refuse.  The identical CFG with
// the field repr'd f64 is the control: it must merge.
function buildReexecAttack(sRepr: "boxed" | "f64"): OptStats {
    const shapeKey = `x:f64,s:${sRepr}`;
    const fb = new FunctionBuilder("reexec", ["%env", "%this", "p"]);
    const p = fb.fn.entry!.params[2]!;

    const fast1 = fb.newBlock("fast1");
    const slow1 = fb.newBlock("slow1");
    const j1 = fb.newBlock("j1");
    const v1 = j1.addParam("v1");
    const g1 = fb.emit("has_shape", [p], { shape: shapeKey });
    fb.condBr(g1, fast1, [], slow1, []);
    fb.sealBlock(fast1);
    fb.sealBlock(slow1);
    fb.setInsertPoint(fast1);
    const lx = fb.emit("slot_load", [p], { shape: shapeKey, slot: 0, repr: "f64" });
    lx.type = "f64";
    const ls = fb.emit("slot_load", [p], { shape: shapeKey, slot: 1, repr: sRepr });
    let sraw: Inst;
    if (sRepr === "boxed") {
        sraw = fb.emit("unbox_f64", [ls], {});
    } else {
        ls.type = "f64";
        sraw = ls;
    }
    const m = fb.emit("f64_mul", [lx, sraw], {});
    fb.br(j1, [fb.emit("box_f64", [m], {})]);
    fb.setInsertPoint(slow1);
    const gx = fb.emit("get_prop_atom", [p], { atom: "x" });
    const gs = fb.emit("get_prop_atom", [p], { atom: "s" });
    const mslow = fb.emit("mul", [gx, gs], {});
    fb.br(j1, [mslow]);
    fb.sealBlock(j1);
    fb.setInsertPoint(j1);

    const fast2 = fb.newBlock("fast2");
    const slow2 = fb.newBlock("slow2");
    const j2 = fb.newBlock("j2");
    const v2 = j2.addParam("v2");
    const g2 = fb.emit("has_shape", [p], { shape: shapeKey });
    fb.condBr(g2, fast2, [], slow2, []);
    fb.sealBlock(fast2);
    fb.sealBlock(slow2);
    fb.setInsertPoint(fast2);
    const l2 = fb.emit("slot_load", [p], { shape: shapeKey, slot: 0, repr: "f64" });
    l2.type = "f64";
    fb.br(j2, [fb.emit("box_f64", [l2], {})]);
    fb.setInsertPoint(slow2);
    const g2x = fb.emit("get_prop_atom", [p], { atom: "x" });
    fb.br(j2, [g2x]);
    fb.sealBlock(j2);
    fb.setInsertPoint(j2);
    fb.ret(fb.emit("add", [v1, v2], {}));

    const fn = fb.finish();
    const mod = new Module("reexec_mod");
    mod.addFunction(fn);
    mod.internShape([
        { name: "x", repr: "f64" },
        { name: "s", repr: sRepr },
    ]);
    verifyModule(mod);
    const stats = shapeOptStats();
    optimizeShapeRegions(fn, mod, stats);
    verifyModule(mod);
    return stats;
}

test("shapes-typed: a boxed-field get feeding slow arithmetic refuses re-execution", () => {
    const refused = buildReexecAttack("boxed");
    assert(
        refused.shape_regions_merged === 0 && refused.shape_numeric_merged === 0,
        "boxed-fed slow arithmetic must refuse the merge"
    );
    const control = buildReexecAttack("f64");
    assert(control.shape_regions_merged === 1, "the f64-repr control must merge");
});

// --- born with their shape -----------------------------------

test("born-shaped: a static literal lowers to make_object_shaped under --types", () => {
    const { printed } = lowerWithOracle(
        "function f(a) { return { x: 1, y: a }; }",
        stubOracle({ a: ["number"] })
    );
    assertContains(printed, "make_object_shaped");
    assertContains(printed, 'shape="x:f64,y:f64"');
    assertNotContains(printed, "make_object ");
});

test("born-shaped: flag-off (null oracle) keeps today's make_object exactly", () => {
    const { printed } = lowerWithOracle("function f(a) { return { x: 1, y: a }; }", null);
    assertNotContains(printed, "make_object_shaped");
    assertContains(printed, "make_object");
});

test("born-shaped: EJS_NO_BORN_SHAPED restores make_object", () => {
    process.env["EJS_NO_BORN_SHAPED"] = "1";
    try {
        const { printed } = lowerWithOracle(
            "function f() { return { x: 1, y: 2 }; }",
            stubOracle({})
        );
        assertNotContains(printed, "make_object_shaped");
    } finally {
        delete process.env["EJS_NO_BORN_SHAPED"];
    }
});

test("born-shaped: index-looking and duplicate keys decline to make_object", () => {
    const dup = lowerWithOracle('function f() { return { x: 1, x: 2 }; }', stubOracle({}));
    assertNotContains(dup.printed, "make_object_shaped");
    const idx = lowerWithOracle('function f() { return { "0": 1, y: 2 }; }', stubOracle({}));
    assertNotContains(idx.printed, "make_object_shaped");
});

test("born-shaped: computed keys / accessors / __proto__ keep the store path", () => {
    const comp = lowerWithOracle("function f(k) { return { [k]: 1, y: 2 }; }", stubOracle({}));
    assertNotContains(comp.printed, "make_object_shaped");
    const acc = lowerWithOracle(
        "function f() { return { get x() { return 1; } }; }",
        stubOracle({})
    );
    assertNotContains(acc.printed, "make_object_shaped");
    const proto = lowerWithOracle(
        "function f(p) { return { __proto__: p, y: 2 }; }",
        stubOracle({})
    );
    assertNotContains(proto.printed, "make_object_shaped");
});

test("ctor-fill: a straight-line this-store prefix lowers to the guarded fill", () => {
    const { printed } = lowerWithOracle(
        "function Pt(x, y) { this.x = x; this.y = y; }",
        stubOracle({ x: ["number"], y: ["number"] })
    );
    assertContains(printed, 'has_shape');
    assertContains(printed, 'shape=""'); // the empty-shape guard
    assertContains(printed, "fill_object_shaped");
    assertContains(printed, 'shape="x:f64,y:f64"');
    assertContains(printed, "ctor_fill_slow");
    assertContains(printed, "set_prop_atom"); // the sequential slow arm survives
});

test("ctor-fill: flag-off keeps the sequential stores exactly", () => {
    const { printed } = lowerWithOracle("function Pt(x, y) { this.x = x; this.y = y; }", null);
    assertNotContains(printed, "fill_object_shaped");
    assertNotContains(printed, "has_shape");
    assertContains(printed, "set_prop_atom");
});

test("ctor-fill: a call-valued store cuts the prefix (fence, oracle-free)", () => {
    // `this.y = g()` could observe the receiver via g — the prefix must
    // stop before it even though a lying oracle calls everything a number
    const { printed } = lowerWithOracle(
        "function Pt(x, g) { this.x = x; this.y = g(); }",
        stubOracle({ x: ["number"], y: ["number"], g: ["number"] })
    );
    assertNotContains(printed, "fill_object_shaped");
});

test("ctor-fill: `in` mid-prefix cuts the batch (the mid-construction observable)", () => {
    const { printed } = lowerWithOracle(
        'function Pt(x, y) { this.x = x; this.t = "y" in this; this.y = y; }',
        stubOracle({ x: ["number"], y: ["number"] })
    );
    assertNotContains(printed, "fill_object_shaped");
});

test("ctor-fill: an escaping receiver before the stores declines", () => {
    const { printed } = lowerWithOracle(
        "function Pt(x, g) { g(this); this.x = x; this.y = x; }",
        stubOracle({ x: ["number"] })
    );
    assertNotContains(printed, "fill_object_shaped");
});

test("ctor-fill: a single-store prefix stays sequential (threshold)", () => {
    const { printed } = lowerWithOracle(
        "function Pt(x) { this.x = x; }",
        stubOracle({ x: ["number"] })
    );
    assertNotContains(printed, "fill_object_shaped");
});

test("ctor-fill: EJS_NO_BORN_SHAPED disables the fill diamond", () => {
    process.env["EJS_NO_BORN_SHAPED"] = "1";
    try {
        const { printed } = lowerWithOracle(
            "function Pt(x, y) { this.x = x; this.y = y; }",
            stubOracle({})
        );
        assertNotContains(printed, "fill_object_shaped");
    } finally {
        delete process.env["EJS_NO_BORN_SHAPED"];
    }
});

// --- born-shaped verifier rules (hand-built attack IR) --------------------------

interface FillAttackOpts {
    guarded?: boolean; // guard the fill with has_shape(recv, "") (default true)
    killInFast?: boolean; // a call between the guard and the fill
    wrongCount?: boolean; // operand count != shape field count
    guardShape?: string; // guard against this shape instead of ""
}

function buildFillAttack(o: FillAttackOpts): Module {
    const guarded = o.guarded !== false;
    const fb = new FunctionBuilder("fillattack", ["%env", "%this", "a", "b"]);
    const recv = fb.fn.entry!.params[1]!;
    const a = fb.fn.entry!.params[2]!;
    const bV = fb.fn.entry!.params[3]!;
    const shapeKey = "x:boxed,y:boxed";

    const fast = fb.newBlock("fast");
    const slow = fb.newBlock("slow");
    const join = fb.newBlock("join");
    const cond = guarded
        ? fb.emit("has_shape", [recv], { shape: o.guardShape ?? "" })
        : fb.emit("to_boolean", [recv], {});
    fb.condBr(cond, fast, [], slow, []);
    fb.sealBlock(fast);
    fb.sealBlock(slow);

    fb.setInsertPoint(fast);
    if (o.killInFast) fb.emit("call_runtime", [], { name: "ToString" });
    const vals = o.wrongCount ? [a] : [a, bV];
    fb.emit("fill_object_shaped", [recv, ...vals], { shape: shapeKey });
    fb.br(join, []);

    fb.setInsertPoint(slow);
    fb.emit("set_prop_atom", [recv, a], { atom: "x" });
    fb.emit("set_prop_atom", [recv, bV], { atom: "y" });
    fb.br(join, []);

    fb.sealBlock(join);
    fb.setInsertPoint(join);
    fb.ret(fb.constUndefined());

    const mod = new Module("fillattack_mod");
    mod.addFunction(fb.finish());
    mod.internShape([]);
    mod.internShape([
        { name: "x", repr: "boxed" },
        { name: "y", repr: "boxed" },
    ]);
    return mod;
}

test("born-verify: a guarded fill in the empty-guard's true arm verifies", () => {
    verifyModule(buildFillAttack({}));
});

test("born-verify: a fill without the empty-shape fact is rejected", () => {
    assertThrows(() => verifyModule(buildFillAttack({ guarded: false })), "empty shape");
});

test("born-verify: a WRITE|CALL between guard and fill kills the fact", () => {
    assertThrows(() => verifyModule(buildFillAttack({ killInFast: true })), "empty shape");
});

test("born-verify: a non-empty guard shape does not license the fill", () => {
    // guarding has_shape(recv, "x:boxed,y:boxed") proves the receiver is
    // FULL, not empty — batching stores onto it would double-install
    assertThrows(
        () => verifyModule(buildFillAttack({ guardShape: "x:boxed,y:boxed" })),
        "empty shape"
    );
});

test("born-verify: operand count must match the shape's field count", () => {
    assertThrows(() => verifyModule(buildFillAttack({ wrongCount: true })), "values for shape");
});

test("born-verify: make_object_shaped checks field count and known shape", () => {
    const fb = new FunctionBuilder("mkattack", ["%env", "%this", "a"]);
    const a = fb.fn.entry!.params[2]!;
    fb.emit("make_object_shaped", [a], { shape: "x:boxed,y:boxed" });
    fb.ret(fb.constUndefined());
    const mod = new Module("mkattack_mod");
    mod.addFunction(fb.finish());
    mod.internShape([
        { name: "x", repr: "boxed" },
        { name: "y", repr: "boxed" },
    ]);
    assertThrows(() => verifyModule(mod), "values for shape");

    const fb2 = new FunctionBuilder("mkattack2", ["%env", "%this", "a"]);
    const a2 = fb2.fn.entry!.params[2]!;
    fb2.emit("make_object_shaped", [a2], { shape: "nope:boxed" });
    fb2.ret(fb2.constUndefined());
    const mod2 = new Module("mkattack2_mod");
    mod2.addFunction(fb2.finish());
    assertThrows(() => verifyModule(mod2), "unknown module shape");
});

// the optimizer/verifier proof-strength hazard (found by
// types-bornshapewrong1): foldProvenGuards deletes a has_tag over a
// const-number join (`c ? 1 : 0`), uncovering the slot_store.  The typed-store form's
// typed store dissolves the hazard class: the store takes a raw f64
// (unbox under whatever proof lowering had), so no guard deletion can
// ever strip the proof — the TYPE is the proof.  Pin both directions:
// the raw form verifies with no has_tag anywhere, the boxed form is
// rejected by type no matter what the join's edges carry.
function buildConstJoinStore(nonNumberEdge: boolean, raw = false): Module {
    const fb = new FunctionBuilder("cjstore", ["%env", "%this", "p", "c"]);
    const p = fb.fn.entry!.params[2]!;
    const c = fb.fn.entry!.params[3]!;
    const shapeKey = "x:f64,y:f64";
    const then_bb = fb.newBlock("then");
    const else_bb = fb.newBlock("else");
    const vjoin = fb.newBlock("vjoin");
    const v = vjoin.addParam("v");
    const fast = fb.newBlock("fast");
    const out = fb.newBlock("out");
    const cb = fb.emit("to_boolean", [c], {});
    fb.condBr(cb, then_bb, [], else_bb, []);
    fb.sealBlock(then_bb);
    fb.sealBlock(else_bb);
    fb.setInsertPoint(then_bb);
    fb.br(vjoin, [fb.constNumber(1)]);
    fb.setInsertPoint(else_bb);
    fb.br(vjoin, [nonNumberEdge ? fb.constUndefined() : fb.constNumber(0)]);
    fb.sealBlock(vjoin);
    fb.setInsertPoint(vjoin);
    const g = fb.emit("has_shape", [p], { shape: shapeKey });
    fb.condBr(g, fast, [], out, []);
    fb.sealBlock(fast);
    fb.setInsertPoint(fast);
    // no has_tag anywhere: the raw form's proof is the operand type
    const stored = raw ? fb.emit("unbox_f64", [v], {}) : v;
    fb.emit("slot_store", [p, stored], { shape: shapeKey, slot: 0, repr: "f64" });
    fb.br(out, []);
    fb.sealBlock(out);
    fb.setInsertPoint(out);
    fb.ret(fb.constUndefined());
    const mod = new Module("cjstore_mod");
    mod.addFunction(fb.finish());
    mod.internShape([
        { name: "x", repr: "f64" },
        { name: "y", repr: "f64" },
    ]);
    return mod;
}

test("born-verify: a typed f64 store needs no has_tag, whatever the join", () => {
    verifyModule(buildConstJoinStore(false, true));
    verifyModule(buildConstJoinStore(true, true));
});

test("born-verify: a boxed value into an f64 slot rejects by type", () => {
    assertThrows(() => verifyModule(buildConstJoinStore(false)), "raw f64");
    assertThrows(() => verifyModule(buildConstJoinStore(true)), "raw f64");
});

// --- shaped-literal sinking -----------------------------------

function lowerShapedSink(src: string): { fn: Func; printed: string; stats: OptStats } {
    const r = lowerFunctionNode(
        parseFn(src),
        undefined,
        stubShapeOracle({ o: PXY }, { a: ["number"] })
    );
    verifyModule(r.module);
    const stats = optimizeFunction(r.fn, r.module);
    verifyModule(r.module);
    return { fn: r.fn, printed: printFunction(r.fn), stats };
}

test("sink-shaped: a non-escaping guarded literal scalar-replaces completely", () => {
    // o's literal is born with PXY's exact shape (a types as number, b is
    // boxed); every read folds to an operand, every guard resolves, the
    // allocation drains away
    const { printed, stats } = lowerShapedSink(
        "function f(a, b) { var o = { x: 1, y: a, s: b }; return o.x + o.y + o.s; }"
    );
    assert(stats.shape_allocs_sunk === 1, `sunk=${stats.shape_allocs_sunk}`);
    assert(stats.shape_guards_sunk >= 1, `guards=${stats.shape_guards_sunk}`);
    assertNotContains(printed, "make_object_shaped");
    assertNotContains(printed, "has_shape");
    assertNotContains(printed, "slot_load");
    assertNotContains(printed, "get_prop_atom");
});

test("sink-shaped: an escaping literal is untouched", () => {
    const { printed, stats } = lowerShapedSink(
        "function f(a, b) { var o = { x: 1, y: a, s: b }; return o; }"
    );
    assert(stats.shape_allocs_sunk === 0, `sunk=${stats.shape_allocs_sunk}`);
    assertContains(printed, "make_object_shaped");
});

test("sink-shaped: a call-operand use escapes", () => {
    const { printed, stats } = lowerShapedSink(
        "function f(a, b, g) { var o = { x: 1, y: a, s: b }; g(o); return o.x; }"
    );
    assert(stats.shape_allocs_sunk === 0, `sunk=${stats.shape_allocs_sunk}`);
    assertContains(printed, "make_object_shaped");
});

test("sink-shaped: a written literal flow-sinks through the generic arms (sinking-P3)", () => {
    // the store's diamond guards fold FALSE (twin arms; sound under
    // writes), the generic read folds to the written const, and the
    // allocation drains
    const { fn, printed, stats } = lowerShapedSink(
        "function f(a, b) { var o = { x: 1, y: a, s: b }; o.x = 2; return o.x; }"
    );
    assert(stats.flow_allocs_sunk === 1, `flow_sunk=${stats.flow_allocs_sunk}`);
    assertNotContains(printed, "make_object_shaped");
    assertNotContains(printed, "slot_store");
    assertNotContains(printed, "set_prop_atom");
    assertNotContains(printed, "get_prop_atom");
    // the written const reaches the return (possibly through the read
    // diamond's now-single-pred join param — LLVM collapses those)
    assertContains(printed, 'value=2');
});

test("sink-shaped: EJS_NO_FLOW_SINK restores the written-literal decline", () => {
    process.env["EJS_NO_FLOW_SINK"] = "1";
    try {
        const { printed, stats } = lowerShapedSink(
            "function f(a, b) { var o = { x: 1, y: a, s: b }; o.x = 2; return o.x; }"
        );
        assert(stats.shape_allocs_sunk === 0, `sunk=${stats.shape_allocs_sunk}`);
        assertContains(printed, "make_object_shaped");
    } finally {
        delete process.env["EJS_NO_FLOW_SINK"];
    }
});

test("sink-shaped: a non-own read blocks removal but own reads still fold", () => {
    const { printed, stats } = lowerShapedSink(
        "function f(a, b) { var o = { x: 1, y: a, s: b }; return o.x + o.zzz; }"
    );
    assert(stats.shape_allocs_sunk === 0, `sunk=${stats.shape_allocs_sunk}`);
    assert(stats.reads_folded >= 1, `folded=${stats.reads_folded}`);
    assertContains(printed, "make_object_shaped");
    assertContains(printed, 'atom="zzz"'); // the prototype read survives
});

test("sink-shaped: shape mismatch resolves guards to the generic arm and still sinks", () => {
    // b is untyped, so the literal's y field is born boxed — its interned
    // shape differs from PXY, every has_shape(o, PXY) is statically false,
    // and the reads fold through the generic arm
    const { printed, stats } = lowerShapedSink(
        "function f(b, c) { var o = { x: 1, y: b, s: c }; return o.x + o.y; }"
    );
    assert(stats.shape_allocs_sunk === 1, `sunk=${stats.shape_allocs_sunk}`);
    assertNotContains(printed, "make_object_shaped");
    assertNotContains(printed, "has_shape");
    assertNotContains(printed, "slot_load");
});

// unprovable-repr attack: the shape KEY matches but an f64 field's operand
// is not provably a number (only buildable by hand — lowering derives repr
// and provability from the same predicate).  the guard must fold FALSE:
// folding true would feed a raw slot_load from a possibly-non-number.
test("sink-shaped: an unprovable f64 operand folds the guard to the generic arm", () => {
    const fb = new FunctionBuilder("unprovable", ["%env", "%this", "v"]);
    const v = fb.fn.entry!.params[2]!;
    const shapeKey = "x:f64,y:f64";
    const alloc = fb.emit("make_object_shaped", [v, v], { shape: shapeKey });
    const fast = fb.newBlock("fast");
    const slow = fb.newBlock("slow");
    const j = fb.newBlock("j");
    const jp = j.addParam("r");
    const g = fb.emit("has_shape", [alloc], { shape: shapeKey });
    fb.condBr(g, fast, [], slow, []);
    fb.sealBlock(fast);
    fb.sealBlock(slow);
    fb.setInsertPoint(fast);
    const l = fb.emit("slot_load", [alloc], { shape: shapeKey, slot: 0, repr: "f64" });
    l.type = "f64";
    fb.br(j, [fb.emit("box_f64", [l], {})]);
    fb.setInsertPoint(slow);
    fb.br(j, [fb.emit("get_prop_atom", [alloc], { atom: "x" })]);
    fb.sealBlock(j);
    fb.setInsertPoint(j);
    fb.ret(jp);
    const fn = fb.finish();
    const mod = new Module("unprovable_mod");
    mod.addFunction(fn);
    mod.internShape([
        { name: "x", repr: "f64" },
        { name: "y", repr: "f64" },
    ]);
    verifyModule(mod);
    const stats = optimizeFunction(fn, mod);
    verifyModule(mod);
    const printed = printFunction(fn);
    assert(stats.shape_guards_sunk === 1, `guards=${stats.shape_guards_sunk}`);
    assert(stats.shape_allocs_sunk === 1, `sunk=${stats.shape_allocs_sunk}`);
    // the raw fast arm must be gone (folding true would have kept it)
    assertNotContains(printed, "slot_load");
    assertNotContains(printed, "make_object_shaped");
    assertNotContains(printed, "get_prop_atom"); // generic arm folded to v
});

test("sink-shaped: EJS_NO_SHAPED_SINK leaves the allocation alone", () => {
    process.env["EJS_NO_SHAPED_SINK"] = "1";
    try {
        const { printed, stats } = lowerShapedSink(
            "function f(a, b) { var o = { x: 1, y: a, s: b }; return o.x + o.y; }"
        );
        assert(stats.shape_allocs_sunk === 0, `sunk=${stats.shape_allocs_sunk}`);
        assertContains(printed, "make_object_shaped");
    } finally {
        delete process.env["EJS_NO_SHAPED_SINK"];
    }
});

// --- flow-sensitive sinking + partial escapes (sinking-P3) ------------------

test("sink-flow: writes across branches fold through a minted join param", () => {
    let { printed } = lowerAndOptimize(
        "function f(c, x, y) { let o = { a: 0 }; if (c) o.a = x; else o.a = y; return o.a; }"
    );
    assertNotContains(printed, "make_object");
    assertNotContains(printed, "set_prop_atom");
    assertNotContains(printed, "get_prop_atom");
});

test("sink-flow: a loop accumulator object drains (loop-carried param)", () => {
    let { printed } = lowerAndOptimize(
        "function f(n) { let o = { sum: 0 }; for (let i = 0; i < n; i = i + 1) o.sum = o.sum + i; return o.sum; }"
    );
    assertNotContains(printed, "make_object");
    assertNotContains(printed, "set_prop_atom");
    assertNotContains(printed, "get_prop_atom");
});

test("sink-flow: a read before the write sees the initial value", () => {
    let { fn, printed } = lowerAndOptimize(
        "function f(x) { let o = { a: 5 }; let r = o.a; o.a = x; return r; }"
    );
    assertNotContains(printed, "make_object");
    let ret: Inst | null = null;
    fn.forEachInst((i) => { if (i.op === "return") ret = i; });
    assert(
        ret!.operands[0]!.op === "const" && ret!.operands[0]!.imms.value === 5,
        `expected the initial 5, got ${ret!.operands[0]!.op}`
    );
});

test("sink-flow: single escape materializes at the escape site", () => {
    // the write is baked into the materialized literal; the original
    // allocation and store are gone but a make_object survives AT the
    // call
    let { fn, printed } = lowerAndOptimize(
        "function f(g, x) { let o = { a: 1 }; o.a = x; g(o); return 0; }"
    );
    assertContains(printed, "make_object");
    assertNotContains(printed, "set_prop_atom");
    // the materialized literal's operand is the written value (param x)
    let made: Inst | null = null;
    fn.forEachInst((i) => { if (i.op === "make_object") made = i; });
    assert(made!.operands[0]!.op === "blockparam", "materialized field should be the written x");
});

test("sink-flow: refusals leave the object alone", () => {
    const cases: [string, string][] = [
        // a read reachable from the escape (the alias could mutate)
        ["use after escape", "function f(g) { let o = { a: 1 }; o.a = 2; g(o); return o.a; }"],
        // the escape can re-execute without re-executing the alloc
        ["escape in loop", "function f(g, n) { let o = { a: 1 }; o.a = 2; for (let i = 0; i < n; i = i + 1) g(o); return 0; }"],
        // two distinct escape instructions
        ["two escapes", "function f(g, h, c) { let o = { a: 1 }; o.a = 2; if (c) g(o); else h(o); return 0; }"],
        // key-adding write ([[Set]] walks the prototype chain)
        ["key-adding write", "function f(x) { let o = { a: 1 }; o.b = x; return 0; }"],
        // the escape instruction is itself a write (o.self = o)
        ["self-write escape", "function f() { let o = { a: 1 }; o.a = o; return 0; }"],
    ];
    for (const [name, src] of cases) {
        let { printed } = lowerAndOptimize(src);
        if (printed.indexOf("make_object") === -1)
            throw new Error(`refusal '${name}' unexpectedly sank\n---\n${printed}\n---`);
    }
});

test("sink-flow: a catch block in the rename region declines", () => {
    let { printed } = lowerAndOptimize(
        "function f(x) { let o = { a: 1 }; try { o.a = x; } catch (e) { } return o.a; }"
    );
    assertContains(printed, "make_object");
});

test("sink-flow: shaped partial escape materializes a shaped literal", () => {
    const { printed, stats } = lowerShapedSink(
        "function f(a, b, g) { var o = { x: 1, y: a, s: b }; o.x = 2; g(o); return 0; }"
    );
    assert(stats.flow_allocs_sunk === 1, `flow_sunk=${stats.flow_allocs_sunk}`);
    assert(stats.allocs_materialized === 1, `materialized=${stats.allocs_materialized}`);
    assertContains(printed, "make_object_shaped"); // the materialized one
    assertNotContains(printed, "slot_store");
    assertNotContains(printed, "set_prop_atom");
});

// --- rest_args / args_obj length sinking (sinking-P3) -----------------------

test("sink-args: length-only arguments folds to arg_len and drains", () => {
    let { printed } = lowerAndOptimize("function f() { return arguments.length; }");
    assertContains(printed, "arg_len");
    assertNotContains(printed, "args_obj");
});

test("sink-args: length-only rest folds with its start index", () => {
    let { printed } = lowerAndOptimize("function f(a, b, ...rest) { return rest.length; }");
    assertContains(printed, "arg_len");
    assertContains(printed, "index=2");
    assertNotContains(printed, "rest_args");
});

test("sink-args: refusals keep the allocation", () => {
    const cases = [
        "function f() { return arguments[0]; }", // computed read
        "function f() { return arguments; }", // escape
        "function f(...r) { r.length = 0; return r.length; }", // length write
        "function f(...r) { return r.length + r[0]; }", // partial fold is not enough
    ];
    for (const src of cases) {
        let { printed } = lowerAndOptimize(src);
        assertNotContains(printed, "arg_len");
    }
});

test("sink-args: EJS_NO_ARGS_SINK leaves the allocation alone", () => {
    process.env["EJS_NO_ARGS_SINK"] = "1";
    try {
        let { printed } = lowerAndOptimize("function f() { return arguments.length; }");
        assertContains(printed, "args_obj");
        assertNotContains(printed, "arg_len");
    } finally {
        delete process.env["EJS_NO_ARGS_SINK"];
    }
});

// --- constructor-result sinking ---------------------------------

// a hand-built module in the shape the sink requires: a fence-passing
// ctor, its closure stored once into promoted %self slot 0 by the
// toplevel, and a consumer constructing through the slot with guarded
// reads.  the knobs each break exactly one screen.
interface CtorSinkOpts {
    secondStore?: boolean; // a second store to the slot
    protoWrite?: boolean; // a load used as a set_prop_atom base
    trailingCtorCode?: boolean; // extra work after the ctor's fill join
    swappedFill?: boolean; // fill operands not the formals in order
    argcMismatch?: boolean; // construct passes fewer args than formals
    escape?: boolean; // result also flows into a call
    twoDiamonds?: boolean; // interleaved add whose value crosses the exit
}

function buildCtorSinkModule(opts: CtorSinkOpts): { mod: Module; user: Func } {
    const mod = new Module("ctor_sink_mod");
    const PXYKey = mod.internShape([
        { name: "x", repr: "f64" },
        { name: "y", repr: "f64" },
    ]);
    mod.internShape([]);

    const cb = new FunctionBuilder("Point", ["%env", "%this", "x", "y"]);
    const cthis = cb.fn.entry!.params[1]!;
    const cx = cb.fn.entry!.params[2]!;
    const cy = cb.fn.entry!.params[3]!;
    const cfast = cb.newBlock("ctor_fill_fast");
    const cslow = cb.newBlock("ctor_fill_slow");
    const cjoin = cb.newBlock("ctor_fill_join");
    const cg = cb.emit("has_shape", [cthis], { shape: "" });
    cb.condBr(cg, cfast, [], cslow, []);
    cb.sealBlock(cfast);
    cb.sealBlock(cslow);
    cb.setInsertPoint(cfast);
    cb.emit("fill_object_shaped", opts.swappedFill ? [cthis, cy, cx] : [cthis, cx, cy], {
        shape: PXYKey,
    });
    cb.br(cjoin, []);
    cb.setInsertPoint(cslow);
    cb.emit("set_prop_atom", [cthis, cx], { atom: "x" });
    cb.emit("set_prop_atom", [cthis, cy], { atom: "y" });
    cb.br(cjoin, []);
    cb.sealBlock(cjoin);
    cb.setInsertPoint(cjoin);
    if (opts.trailingCtorCode) cb.emit("get_prop_atom", [cthis], { atom: "x" });
    cb.ret(cb.constUndefined());
    mod.addFunction(cb.finish());

    const tb = new FunctionBuilder("toplevel", ["%env", "%this"]);
    const tenv = tb.fn.entry!.params[0]!;
    const cl = tb.emit("make_closure", [tenv], { fn: "Point", name: "Point" });
    tb.emit("module_slot_store", [cl], { module: "%self", slot: 0 });
    if (opts.secondStore) tb.emit("module_slot_store", [cl], { module: "%self", slot: 0 });
    if (opts.protoWrite) {
        const ld = tb.emit("module_slot_load", [], { module: "%self", slot: 0 });
        tb.emit("set_prop_atom", [ld, tb.constNumber(1)], { atom: "prototype" });
    }
    tb.ret(tb.constUndefined());
    mod.addFunction(tb.finish());

    const ub = new FunctionBuilder("user", ["%env", "%this", "g"]);
    const gparam = ub.fn.entry!.params[2]!;
    const ld = ub.emit("module_slot_load", [], { module: "%self", slot: 0 });
    const bx = ub.emit("box_f64", [ub.emit("f64_const", [], { value: 1 })], {});
    const by = ub.emit("box_f64", [ub.emit("f64_const", [], { value: 2 })], {});
    const p = ub.emit("construct", opts.argcMismatch ? [ld, bx] : [ld, bx, by], {});

    const fast = ub.newBlock("shape_fast");
    const slow = ub.newBlock("shape_slow");
    const join = ub.newBlock("shape_join");
    const r = join.addParam("r");
    const pg = ub.emit("has_shape", [p], { shape: PXYKey });
    ub.condBr(pg, fast, [], slow, []);
    ub.sealBlock(fast);
    ub.sealBlock(slow);
    ub.setInsertPoint(fast);
    const sl = ub.emit("slot_load", [p], { shape: PXYKey, slot: 0, repr: "f64" });
    sl.type = "f64";
    ub.br(join, [ub.emit("box_f64", [sl], {})]);
    ub.setInsertPoint(slow);
    ub.br(join, [ub.emit("get_prop_atom", [p], { atom: "x" })]);
    ub.sealBlock(join);
    ub.setInsertPoint(join);

    if (opts.twoDiamonds) {
        // a value defined between the diamonds and used past the exit —
        // it must cross the epoch join through a minted param
        const s = ub.emit("add", [r, bx], {});
        const fast2 = ub.newBlock("shape_fast2");
        const slow2 = ub.newBlock("shape_slow2");
        const join2 = ub.newBlock("shape_join2");
        const r2 = join2.addParam("r2");
        const pg2 = ub.emit("has_shape", [p], { shape: PXYKey });
        ub.condBr(pg2, fast2, [], slow2, []);
        ub.sealBlock(fast2);
        ub.sealBlock(slow2);
        ub.setInsertPoint(fast2);
        const sl2 = ub.emit("slot_load", [p], { shape: PXYKey, slot: 1, repr: "f64" });
        sl2.type = "f64";
        ub.br(join2, [ub.emit("box_f64", [sl2], {})]);
        ub.setInsertPoint(slow2);
        ub.br(join2, [ub.emit("get_prop_atom", [p], { atom: "y" })]);
        ub.sealBlock(join2);
        ub.setInsertPoint(join2);
        ub.ret(ub.emit("add", [s, r2], {}));
    } else {
        if (opts.escape) ub.emit("call", [gparam, ub.constUndefined(), p], {});
        ub.ret(r);
    }
    const user = ub.finish();
    mod.addFunction(user);
    return { mod, user };
}

function runCtorSink(opts: CtorSinkOpts = {}): { n: number; printed: string; stats: OptStats } {
    const { mod, user } = buildCtorSinkModule(opts);
    verifyModule(mod);
    const n = sinkConstructResults(mod, new Set([0]), "toplevel");
    verifyModule(mod);
    const stats = optimizeFunction(user, mod);
    verifyModule(mod);
    return { n, printed: printFunction(user), stats };
}

test("sink-ctor: a qualifying construct virtualizes behind the epoch check", () => {
    const { n, printed, stats } = runCtorSink({});
    assert(n === 1, `sunk=${n}`);
    assertContains(printed, "epoch_check");
    assertContains(printed, "construct"); // the slow arm keeps the real one
    assertNotContains(printed, "make_object_shaped"); // the virtual arm drained
    assert(stats.shape_allocs_sunk === 1, `allocs=${stats.shape_allocs_sunk}`);
});

test("sink-ctor: live-outs cross the epoch join through minted params", () => {
    const { n, printed, stats } = runCtorSink({ twoDiamonds: true });
    assert(n === 1, `sunk=${n}`);
    assertContains(printed, "epoch_check");
    assertNotContains(printed, "make_object_shaped");
    assert(stats.shape_allocs_sunk === 1, `allocs=${stats.shape_allocs_sunk}`);
});

test("sink-ctor: refusals leave the construct alone", () => {
    const attacks: CtorSinkOpts[] = [
        { secondStore: true },
        { protoWrite: true },
        { trailingCtorCode: true },
        { swappedFill: true },
        { argcMismatch: true },
        { escape: true },
    ];
    for (const a of attacks) {
        const { n, printed } = runCtorSink(a);
        assert(n === 0, `${JSON.stringify(a)}: sunk=${n}`);
        assertNotContains(printed, "epoch_check");
    }
});

test("sink-ctor: a non-promoted slot declines", () => {
    const { mod, user } = buildCtorSinkModule({});
    verifyModule(mod);
    const n = sinkConstructResults(mod, new Set<number>(), "toplevel");
    assert(n === 0, `sunk=${n}`);
    verifyModule(mod);
    assertNotContains(printFunction(user), "epoch_check");
});

test("sink-ctor: EJS_NO_CTOR_SINK leaves the construct alone", () => {
    process.env["EJS_NO_CTOR_SINK"] = "1";
    try {
        const { n, printed } = runCtorSink({});
        assert(n === 0, `sunk=${n}`);
        assertNotContains(printed, "epoch_check");
    } finally {
        delete process.env["EJS_NO_CTOR_SINK"];
    }
});

// --------------------------------------------------------------------------------

if (failures > 0) {
    console.log(`${failures} test(s) FAILED`);
    process.exit(1);
} else {
    console.log("all EIR tests passed");
}
