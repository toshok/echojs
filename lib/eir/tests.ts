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
import { lowerFunctionNode, lowerProgram } from "./lower";
import { optimizeFunction } from "./optimize";
import { isLowerNotSupported } from "./errors";
import { Func, Block, Inst, Module } from "./ir";
import { DesugarSpread } from "../passes/desugar-spread";
import { typeSigToEirType } from "./oracle";
import type { TypeOracle, TypeTag } from "./oracle";
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

test("optimize: a written key blocks folding its reads", () => {
    let { printed } = lowerAndOptimize("function f(x) { let o = { a: 1 }; o.a = x; return o.a; }");
    assertContains(printed, "make_object");
    assertContains(printed, "get_prop_atom");
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

// --- the typed low tier (Phase 2) ------------------------------------------------

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

// --- Phase 3: oracle-guided guarded arithmetic ------------------------------------

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

// --- Phase 3.4: guard-region merging + raw f64 joins ------------------------------

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
    // as lowered this is three diamonds / six has_tags (see the Phase 3
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

// --------------------------------------------------------------------------------

if (failures > 0) {
    console.log(`${failures} test(s) FAILED`);
    process.exit(1);
} else {
    console.log("all EIR tests passed");
}
