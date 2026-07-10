/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

// EIR unit tests.  run (via the babel'd tree) with:
//   node lib/generated/lib/eir/tests.js
// or through buck:
//   buck2 build //:test-eir

import { FunctionBuilder } from "./builder";
import { printFunction, printModule } from "./printer";
import { verifyFunction, verifyModule } from "./verifier";
import { lowerFunctionNode, lowerProgram } from "./lower";
import { isLowerNotSupported } from "./errors";
import { Func, Block, Inst } from "./ir";
import { DesugarSpread } from "../passes/desugar-spread";
import * as esprima from "../../external-deps/esprima/esprima-es6";

let failures = 0;

function test(name, fn) {
    try {
        fn();
        console.log(`pass: ${name}`);
    } catch (e) {
        failures++;
        console.log(`FAIL: ${name}: ${e.message}`);
        if (e.stack) console.log(e.stack.split("\n").slice(1, 4).join("\n"));
    }
}

function assert(cond, msg) {
    if (!cond) throw new Error(`assertion failed: ${msg || ""}`);
}

function assertContains(haystack, needle) {
    if (haystack.indexOf(needle) === -1)
        throw new Error(`expected output to contain '${needle}'\n---\n${haystack}\n---`);
}

function findBlock(fn, prefix) {
    for (let b of fn.blocks) if (b.name.indexOf(prefix) === 0) return b;
    throw new Error(`no block named ${prefix}* in @${fn.name}`);
}

function findFn(mod, name) {
    for (let f of mod.functions) if (f.name === name) return f;
    throw new Error(`no function @${name} in module`);
}

function parseFn(src) {
    let ast = esprima.parse(src, { loc: true, raw: true });
    for (let s of ast.body) if (s.type === "FunctionDeclaration") return s;
    throw new Error("no function declaration in source");
}

function lowerOne(src) {
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
    assert(catch_bb.params[0].isException, "first catch param is the exception");
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
        lowerFunctionNode(parseFn("function t(x) { lbl: for (;;) { break lbl; } }"));
    } catch (e) {
        threw = isLowerNotSupported(e);
    }
    assert(threw, "expected LowerNotSupported");
});

// --- lowering: %-intrinsics ------------------------------------------------

// parse + DesugarSpread, like the pre-EIR pipeline in compile()
function parseFnSpreadDesugared(src) {
    let ast = esprima.parse(src, { loc: true, raw: true });
    ast = new DesugarSpread({ debug_passes: new Set() }).visit(ast);
    for (let s of ast.body) if (s.type === "FunctionDeclaration") return s;
    throw new Error("no function declaration in source");
}

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

test("lower: unknown %-intrinsics raise LowerNotSupported", () => {
    let fnNode = parseFnSpreadDesugared("function t(a) { return dummy(a); }");
    // synthesize a call to an intrinsic lowering doesn't know
    fnNode.body.body[0].argument.callee.name = "%noSuchIntrinsic";
    let threw = false;
    try {
        lowerFunctionNode(fnNode);
    } catch (e) {
        threw = isLowerNotSupported(e);
    }
    assert(threw, "expected LowerNotSupported");
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
        threw = /does not dominate/.test(e.message);
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
        threw = /no terminator/.test(e.message);
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
        threw = /non-unwind edge into catch/.test(e.message);
    }
    assert(threw, "expected a catch-edge violation");
});

// --------------------------------------------------------------------------------

if (failures > 0) {
    console.log(`${failures} test(s) FAILED`);
    process.exit(1);
} else {
    console.log("all EIR tests passed");
}
