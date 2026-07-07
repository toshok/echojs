/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

// EIR unit tests.  run (via the babel'd tree) with:
//   node lib/generated/lib/eir/tests.js
// or through buck:
//   buck2 build //:test-eir

import { FunctionBuilder } from "./builder";
import { printFunction, printModule } from "./printer";
import { verifyFunction } from "./verifier";
import { lowerFunctionNode, lowerProgram, LowerNotSupported } from "./lower";
import { Func, Block, Inst } from "./ir";
import * as esprima from "../../external-deps/esprima/esprima-es6";

let failures = 0;
let current = "";

function test(name, fn) {
    current = name;
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

function parseFn(src) {
    let ast = esprima.parse(src, { loc: true, raw: true });
    for (let s of ast.body) if (s.type === "FunctionDeclaration") return s;
    throw new Error("no function declaration in source");
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
    let join = findBlock(fn, "join");
    assert(join.params.length === 1, "join should have exactly one param");

    let printed = printFunction(fn);
    assertContains(printed, "const kind=\"number\", value=1");
    assertContains(printed, "const kind=\"number\", value=2");
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

// --- lowering -----------------------------------------------------------------

test("lower: loop-invariant variable gets no header param", () => {
    let fn = lowerFunctionNode(parseFn("function f(c) { let a = 5; while (c) { } return a; }"));
    verifyFunction(fn);
    let header = findBlock(fn, "while_header");
    assert(header.params.length === 0, `header params = ${header.params.length}`);
});

test("lower: loop counter gets exactly one header param", () => {
    let fn = lowerFunctionNode(
        parseFn("function g(n) { let i = 0; while (i < n) { i = i + 1; } return i; }")
    );
    verifyFunction(fn);
    let header = findBlock(fn, "while_header");
    assert(header.params.length === 1, `header params = ${header.params.length}`);
    let printed = printFunction(fn);
    assertContains(printed, "lt");
    assertContains(printed, "add");
});

test("lower: if/else assigns and joins", () => {
    let fn = lowerFunctionNode(
        parseFn("function h(c) { let x = 0; if (c) { x = 1; } else { x = 2; } return x; }")
    );
    verifyFunction(fn);
    let join = findBlock(fn, "if_join");
    assert(join.params.length === 1, `join params = ${join.params.length}`);
});

test("lower: logical && short-circuits through a join param", () => {
    let fn = lowerFunctionNode(parseFn("function a(x, y) { return x && y; }"));
    verifyFunction(fn);
    let printed = printFunction(fn);
    assertContains(printed, "logical_join");
    assertContains(printed, "cond_br");
});

test("lower: method calls and property access", () => {
    let fn = lowerFunctionNode(
        parseFn("function m(o) { o.count = o.count + 1; return o.get(o.count, 3); }")
    );
    verifyFunction(fn);
    let printed = printFunction(fn);
    assertContains(printed, 'get_prop_atom');
    assertContains(printed, 'atom="count"');
    assertContains(printed, 'set_prop_atom');
    assertContains(printed, 'atom="get"');
    assertContains(printed, "call");
});

test("lower: globals resolve to get_global", () => {
    let fn = lowerFunctionNode(parseFn("function p(x) { return console.log(x); }"));
    verifyFunction(fn);
    assertContains(printFunction(fn), 'get_global atom="console"');
});

test("lower: unsupported constructs raise LowerNotSupported", () => {
    let threw = false;
    try {
        lowerFunctionNode(parseFn("function t(x) { try { x(); } catch (e) { } }"));
    } catch (e) {
        threw = e instanceof LowerNotSupported;
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
    let printed = printModule(mod);
    assertContains(printed, "fn @one");
    assertContains(printed, "fn @two");
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

    // b uses a's value: invalid
    let bad = new Inst(fn, "add", [c1, c1], {});
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

// --------------------------------------------------------------------------------

if (failures > 0) {
    console.log(`${failures} test(s) FAILED`);
    process.exit(1);
} else {
    console.log("all EIR tests passed");
}
