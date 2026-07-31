/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// Hand-built low-tier bodies for the low-tier end-to-end test.  Lowering does
// does emit has_tag/unbox_f64/f64_*/box_f64 through the oracle path, but to prove
// the emitted machine code is correct we substitute known bodies into the
// functions of test/eir-lowtier1.js, gated on -flowtier (a debug/test
// hook in the -fno-eir-opt mold).  With the flag off nothing here
// runs; the test file behaves identically either way, so it also passes in
// the normal matrix.
//
// The shape built here is exactly the guarded diamond: has_tag both
// operands -> fast block (unbox / f64 op / box) vs slow block (the generic
// op), joining in a BOXED block parameter (raw f64/i1 never crosses a block
// boundary; the verifier enforces that).

import { FunctionBuilder } from "./builder";
import { verifyFunction } from "./verifier";
import type { Func, Module, Inst, Block } from "./ir";

// `function <name>(a, b) { return a <op> b; }` as the guarded diamond,
// parameterized over the fast f64 op and its generic slow-path twin.
export function buildArithDiamond(name: string, f64Op: string, genericOp: string): Func {
    const fb = new FunctionBuilder(name, ["%env", "%this", "a", "b"]);
    const a = fb.readVariable("a", fb.cur);
    const b = fb.readVariable("b", fb.cur);

    const chk2 = fb.newBlock("chk2");
    const fast = fb.newBlock("fast");
    const slow = fb.newBlock("slow");
    const join = fb.newBlock("join");

    const t1 = fb.emit("has_tag", [a], { tag: "number" });
    fb.condBr(t1, chk2, [], slow, []);
    fb.sealBlock(chk2);

    fb.setInsertPoint(chk2);
    const t2 = fb.emit("has_tag", [b], { tag: "number" });
    fb.condBr(t2, fast, [], slow, []);
    fb.sealBlock(fast);
    fb.sealBlock(slow);

    fb.setInsertPoint(fast);
    const ua = fb.emit("unbox_f64", [a], {});
    const ub = fb.emit("unbox_f64", [b], {});
    const val = fb.emit(f64Op, [ua, ub], {});
    const boxed = fb.emit("box_f64", [val], {});
    fb.writeVariable("res", fast, boxed);
    fb.br(join, []);

    fb.setInsertPoint(slow);
    const generic = fb.emit(genericOp, [a, b], {});
    fb.writeVariable("res", slow, generic);
    fb.br(join, []);
    fb.sealBlock(join);

    fb.setInsertPoint(join);
    fb.ret(fb.readVariable("res", join));

    const fn = fb.finish();
    verifyFunction(fn);
    return fn;
}

export function buildLowTierAdd(name: string): Func {
    return buildArithDiamond(name, "f64_add", "add");
}

// `function <name>(a, b) { return a < b; }`: the fast arm branches on the
// raw i1 from f64_lt and rejoins with boxed booleans.
export function buildLowTierLt(name: string): Func {
    const fb = new FunctionBuilder(name, ["%env", "%this", "a", "b"]);
    const a = fb.readVariable("a", fb.cur);
    const b = fb.readVariable("b", fb.cur);

    const chk2 = fb.newBlock("chk2");
    const fast = fb.newBlock("fast");
    const lt_true = fb.newBlock("lt_true");
    const lt_false = fb.newBlock("lt_false");
    const slow = fb.newBlock("slow");
    const join = fb.newBlock("join");

    const t1 = fb.emit("has_tag", [a], { tag: "number" });
    fb.condBr(t1, chk2, [], slow, []);
    fb.sealBlock(chk2);

    fb.setInsertPoint(chk2);
    const t2 = fb.emit("has_tag", [b], { tag: "number" });
    fb.condBr(t2, fast, [], slow, []);
    fb.sealBlock(fast);
    fb.sealBlock(slow);

    fb.setInsertPoint(fast);
    const ua = fb.emit("unbox_f64", [a], {});
    const ub = fb.emit("unbox_f64", [b], {});
    const lt = fb.emit("f64_lt", [ua, ub], {});
    fb.condBr(lt, lt_true, [], lt_false, []);
    fb.sealBlock(lt_true);
    fb.sealBlock(lt_false);

    fb.setInsertPoint(lt_true);
    fb.writeVariable("res", lt_true, fb.constBool(true));
    fb.br(join, []);

    fb.setInsertPoint(lt_false);
    fb.writeVariable("res", lt_false, fb.constBool(false));
    fb.br(join, []);

    fb.setInsertPoint(slow);
    fb.writeVariable("res", slow, fb.emit("lt", [a, b], {}));
    fb.br(join, []);
    fb.sealBlock(join);

    fb.setInsertPoint(join);
    fb.ret(fb.readVariable("res", join));

    const fn = fb.finish();
    verifyFunction(fn);
    return fn;
}

const PROBES: Array<{ marker: string; build: (name: string) => Func }> = [
    { marker: "lowtier_add", build: (n) => buildArithDiamond(n, "f64_add", "add") },
    { marker: "lowtier_sub", build: (n) => buildArithDiamond(n, "f64_sub", "sub") },
    { marker: "lowtier_mul", build: (n) => buildArithDiamond(n, "f64_mul", "mul") },
    { marker: "lowtier_div", build: (n) => buildArithDiamond(n, "f64_div", "div") },
    { marker: "lowtier_lt", build: buildLowTierLt },
];

// Swap the probe bodies into a lowered module, in place, preserving each
// function's name (make_closure references functions by name).
export function injectLowTierProbes(module: Module): number {
    let injected = 0;
    for (let i = 0; i < module.functions.length; i++) {
        const fn = module.functions[i]!;
        for (const probe of PROBES) {
            if (fn.name.indexOf(probe.marker) === -1) continue;
            module.functions[i] = probe.build(fn.name);
            injected++;
            break;
        }
    }
    return injected;
}

// re-exported for the unit tests' ill-typed-flow constructions
export type { Func, Inst, Block };
