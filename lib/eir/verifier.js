/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
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

function computeRPO(fn) {
    let visited = new Set();
    let postorder = [];
    // iterative dfs to keep the verifier usable on deep CFGs
    let stack = [{ block: fn.entry, succIndex: 0 }];
    visited.add(fn.entry);
    while (stack.length > 0) {
        let frame = stack[stack.length - 1];
        let succs = frame.block.succs();
        if (frame.succIndex < succs.length) {
            let s = succs[frame.succIndex++];
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
function computeDominators(fn, rpo) {
    let index = new Map();
    rpo.forEach((b, i) => index.set(b, i));

    let idom = new Map();
    idom.set(fn.entry, fn.entry);

    let intersect = (a, b) => {
        while (a !== b) {
            while (index.get(a) > index.get(b)) a = idom.get(a);
            while (index.get(b) > index.get(a)) b = idom.get(b);
        }
        return a;
    };

    let changed = true;
    while (changed) {
        changed = false;
        for (let b of rpo) {
            if (b === fn.entry) continue;
            let newIdom = null;
            for (let p of b.preds()) {
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

function dominates(idom, a, b) {
    // does block a dominate block b?
    let runner = b;
    while (true) {
        if (runner === a) return true;
        let next = idom.get(runner);
        if (next === undefined || next === runner) return runner === a;
        runner = next;
    }
}

export function verifyFunction(fn) {
    // inner template hoisted out of the outer template: the legacy
    // compiler miscompiles nested templates through arrows.
    let vname = (v) => `%v${v.id}`;
    let fail = (msg, inst) => {
        let where = "";
        if (inst) {
            let inst_str = printInst(inst, vname);
            where = ` at '${inst_str}'`;
        }
        throw new Error(`EIR verifier: fn @${fn.name}: ${msg}${where}`);
    };

    if (!fn.entry) fail("no entry block");

    let { rpo, reachable } = computeRPO(fn);
    let idom = computeDominators(fn, rpo);

    // per-block structural checks
    for (let b of fn.blocks) {
        if (!b.sealed) fail(`block ^${b.name} is not sealed`);
        if (!reachable.has(b)) continue; // ignore unreachable blocks beyond seal check

        let term = null;
        for (let i = 0; i < b.insts.length; i++) {
            let inst = b.insts[i];
            let info = opInfo(inst.op); // throws on unknown op
            if (info.arity >= 0 && inst.operands.length !== info.arity)
                fail(`'${inst.op}' has ${inst.operands.length} operands, wants ${info.arity}`, inst);
            if (isTerminator(inst)) {
                if (i !== b.insts.length - 1)
                    fail(`terminator in the middle of ^${b.name}`, inst);
                term = inst;
            }
            if (inst.op === "blockparam") fail("blockparam in instruction stream", inst);
        }
        if (!term) fail(`block ^${b.name} has no terminator`);

        // edge argument counts match target params (catch blocks' exception
        // param is produced by unwinding, not passed on the edge)
        if (term.targets) {
            for (let t of term.targets) {
                let expected = t.block.params.length;
                if (t.block.isCatch) {
                    if (t.kind !== "unwind")
                        fail(`non-unwind edge into catch block ^${t.block.name}`, term);
                    if (t.block.params.length === 0 || !t.block.params[0].isException)
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
                for (let a of t.args)
                    if (a === null || a === undefined)
                        fail(`edge to ^${t.block.name} has an unfilled argument`, term);
            }
        }
    }

    // def-dominates-use.  a value used as an operand must be defined in a
    // block that dominates the use block (params count as defined at block
    // entry; straight-line order enforced within a block).
    let instIndex = new Map();
    for (let b of fn.blocks) {
        b.insts.forEach((inst, i) => instIndex.set(inst, i));
    }

    let checkUse = (val, userBlock, userIdx, inst) => {
        if (val.removed) fail("use of removed block parameter", inst);
        let defBlock = val.block;
        if (!reachable.has(defBlock)) fail("operand defined in unreachable block", inst);
        if (defBlock === userBlock) {
            if (val.op === "blockparam") return; // defined at entry of the block
            let defIdx = instIndex.get(val);
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

    for (let b of fn.blocks) {
        if (!reachable.has(b)) continue;
        b.insts.forEach((inst, i) => {
            for (let o of inst.operands) checkUse(o, b, i, inst);
            if (inst.targets) {
                for (let t of inst.targets) for (let a of t.args) checkUse(a, b, i, inst);
            }
        });
    }

    return true;
}

export function verifyModule(mod) {
    for (let fn of mod.functions) verifyFunction(fn);
    return true;
}
