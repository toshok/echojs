/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// EIR structural verifier.  checks:
//   - every opcode exists and respects its arity
//   - every block is sealed and ends in exactly one terminator
//   - branch edge argument counts match the target's parameter counts
//   - every operand's definition dominates its use (standard iterative
//     dominance computation over the CFG)
//
// verify() throws on the first violation; the error message names the
// function, block, and instruction involved.

import { opInfo, isTerminator } from "./ops";
import { printInst } from "./printer";
import type { Func, Block, Inst, Module } from "./ir";

function computeRPO(fn: Func): { rpo: Block[]; reachable: Set<Block> } {
    const entry = fn.entry!;
    const visited = new Set<Block>();
    const postorder: Block[] = [];
    // iterative dfs to keep the verifier usable on deep CFGs
    const stack = [{ block: entry, succIndex: 0 }];
    visited.add(entry);
    while (stack.length > 0) {
        const frame = stack[stack.length - 1]!;
        const succs = frame.block.succs();
        if (frame.succIndex < succs.length) {
            const s = succs[frame.succIndex++]!;
            if (!visited.has(s)) {
                visited.add(s);
                stack.push({ block: s, succIndex: 0 });
            }
        } else {
            postorder.push(frame.block);
            stack.pop();
        }
    }
    return { rpo: postorder.slice().reverse(), reachable: visited };
}

// Cooper/Harvey/Kennedy "A Simple, Fast Dominance Algorithm"
function computeDominators(fn: Func, rpo: Block[]): Map<Block, Block> {
    const entry = fn.entry!;
    const index = new Map<Block, number>();
    rpo.forEach((b, i) => index.set(b, i));

    const idom = new Map<Block, Block>();
    idom.set(entry, entry);

    const intersect = (a: Block, b: Block): Block => {
        while (a !== b) {
            while (index.get(a)! > index.get(b)!) a = idom.get(a)!;
            while (index.get(b)! > index.get(a)!) b = idom.get(b)!;
        }
        return a;
    };

    let changed = true;
    while (changed) {
        changed = false;
        for (const b of rpo) {
            if (b === entry) continue;
            let newIdom: Block | null = null;
            for (const p of b.preds()) {
                if (!index.has(p)) continue; // unreachable pred
                if (!idom.has(p)) continue;
                newIdom = newIdom === null ? p : intersect(p, newIdom);
            }
            if (newIdom !== null && idom.get(b) !== newIdom) {
                idom.set(b, newIdom);
                changed = true;
            }
        }
    }
    return idom;
}

function dominates(idom: Map<Block, Block>, a: Block, b: Block): boolean {
    // does block a dominate block b?
    let runner = b;
    for (;;) {
        if (runner === a) return true;
        const next = idom.get(runner);
        if (next === undefined || next === runner) return runner === a;
        runner = next;
    }
}

export function verifyFunction(fn: Func): boolean {
    const vname = (v: Inst | null | undefined) => (v ? `%v${v.id}` : "<null>");
    const fail = (msg: string, inst?: Inst): never => {
        let where = "";
        if (inst) {
            const inst_str = printInst(inst, vname);
            where = ` at '${inst_str}'`;
        }
        throw new Error(`EIR verifier: fn @${fn.name}: ${msg}${where}`);
    };

    if (!fn.entry) fail("no entry block");

    const { rpo, reachable } = computeRPO(fn);
    const idom = computeDominators(fn, rpo);

    // per-block structural checks
    for (const b of fn.blocks) {
        if (!b.sealed) fail(`block ^${b.name} is not sealed`);
        if (!reachable.has(b)) continue; // ignore unreachable blocks beyond seal check

        let term: Inst | null = null;
        for (let i = 0; i < b.insts.length; i++) {
            const inst = b.insts[i]!;
            const info = opInfo(inst.op); // throws on unknown op
            if (info.arity >= 0 && inst.operands.length !== info.arity)
                fail(`'${inst.op}' has ${inst.operands.length} operands, wants ${info.arity}`, inst);
            if (isTerminator(inst)) {
                if (i !== b.insts.length - 1) fail(`terminator in the middle of ^${b.name}`, inst);
                term = inst;
            }
            if (inst.op === "blockparam") fail("blockparam in instruction stream", inst);
        }
        if (!term) {
            fail(`block ^${b.name} has no terminator`);
            continue;
        }

        // edge argument counts match target params (catch blocks' exception
        // param is produced by unwinding, not passed on the edge)
        if (term.targets) {
            for (const t of term.targets) {
                let expected = t.block.params.length;
                if (t.block.isCatch) {
                    if (t.kind !== "unwind")
                        fail(`non-unwind edge into catch block ^${t.block.name}`, term);
                    if (t.block.params.length === 0 || !t.block.params[0]!.isException)
                        fail(`catch block ^${t.block.name} missing its exception param`, term);
                    expected -= 1;
                } else if (t.kind === "unwind") {
                    fail(`unwind edge into non-catch block ^${t.block.name}`, term);
                }
                if (t.args.length !== expected)
                    fail(
                        `edge to ^${t.block.name} passes ${t.args.length} args, target wants ${expected}`,
                        term
                    );
                for (const a of t.args)
                    if (a === null || a === undefined)
                        fail(`edge to ^${t.block.name} has an unfilled argument`, term);
            }
        }
    }

    // def-dominates-use.  a value used as an operand must be defined in a
    // block that dominates the use block (params count as defined at block
    // entry; straight-line order enforced within a block).
    const instIndex = new Map<Inst, number>();
    for (const b of fn.blocks) {
        b.insts.forEach((inst, i) => instIndex.set(inst, i));
    }

    const checkUse = (val: Inst | null, userBlock: Block, userIdx: number, inst: Inst): void => {
        if (!val) {
            fail("null operand", inst);
            return;
        }
        if (val.removed) fail("use of removed block parameter", inst);
        const defBlock = val.block!;
        if (!reachable.has(defBlock)) fail("operand defined in unreachable block", inst);
        if (defBlock === userBlock) {
            if (val.op === "blockparam") return; // defined at entry of the block
            const defIdx = instIndex.get(val);
            if (defIdx === undefined || defIdx >= userIdx)
                fail(`operand %v${val.id} used before definition`, inst);
        } else {
            if (!dominates(idom, defBlock, userBlock))
                fail(
                    `operand %v${val.id} (def in ^${defBlock.name}) does not dominate use in ^${userBlock.name}`,
                    inst
                );
        }
    };

    for (const b of fn.blocks) {
        if (!reachable.has(b)) continue;
        b.insts.forEach((inst, i) => {
            for (const o of inst.operands) checkUse(o, b, i, inst);
            if (inst.targets) {
                for (const t of inst.targets) for (const a of t.args) checkUse(a, b, i, inst);
            }
        });
    }

    // typed-flow rules (the low tier).  f64/i1 values are raw machine values:
    //   - an op with a sig gets exactly what the sig says per slot ("f64"
    //     slots take only f64 values; "ejsval" slots take any boxed value,
    //     which excludes f64/i1);
    //   - an op without a sig takes only boxed values — with one exception:
    //     cond_br's condition may additionally be i1 (has_tag / f64_lt; the
    //     legacy "any"-typed condition sources to_boolean / prop_iter_next
    //     already emit their own machine i1);
    //   - branch-edge arguments must be boxed: block params are EjsValue
    //     phis in the emitter, so f64/i1 may NOT cross block boundaries.
    //     (Phase 3's guarded diamonds carry values across joins boxed.)
    const isRaw = (t: string) => t === "f64" || t === "i1";
    for (const b of fn.blocks) {
        if (!reachable.has(b)) continue;
        for (const inst of b.insts) {
            const info = opInfo(inst.op);
            inst.operands.forEach((o, idx) => {
                const want = info.sig ? info.sig.params[idx] : undefined;
                if (want === "f64") {
                    if (o.type !== "f64")
                        fail(`'${inst.op}' operand ${idx} wants f64, got ${o.type}`, inst);
                } else if (want === "ejsval") {
                    if (isRaw(o.type))
                        fail(`'${inst.op}' operand ${idx} wants a boxed value, got ${o.type}`, inst);
                } else if (inst.op === "cond_br" && idx === 0) {
                    if (o.type === "f64") fail("cond_br condition may not be f64", inst);
                } else if (isRaw(o.type)) {
                    fail(`'${inst.op}' operand ${idx} may not be ${o.type}`, inst);
                }
            });
            if (inst.targets)
                for (const t of inst.targets)
                    for (const a of t.args)
                        if (a && isRaw(a.type))
                            fail(
                                `edge to ^${t.block.name} passes a raw ${a.type} value; block arguments must be boxed`,
                                inst
                            );
        }
    }

    return true;
}

export function verifyModule(mod: Module): boolean {
    for (const fn of mod.functions) verifyFunction(fn);
    return true;
}
