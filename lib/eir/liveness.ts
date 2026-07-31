/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// which values must live in gc-frame slots?
//
// A value needs a precise, relocatable home iff it is LIVE ACROSS a
// safepoint — an op that lowers to a runtime call that may allocate
// (and therefore may run a minor collection that MOVES young objects).
// Values not live across any safepoint never coexist with a move; raw
// f64/i1 values are not references; consts rematerialize from statics.
//
// Soundness does not depend on this analysis being complete: any value
// left out keeps its SSA home, and a live-across-call SSA value is
// always visible to the conservative stack/register scan (ABI: it must
// be in a callee-saved register or a stack slot), which PINS its
// referent.  Under-coverage costs pins, never correctness.  That is
// also why v1 deliberately skips two classes:
//  - ops with unwind targets (invoke form, try regions): their reload
//    point is the normal edge, which may be shared — skipped values
//    stay pinned;
//  - values DEFINED by target-carrying ops: their def-site store has
//    no natural insertion point in the defining block.

import { Func, Inst } from "./ir";
import { Effect, opInfo } from "./ops";

// a v1 safepoint: a target-less op whose lowering calls into the
// runtime with allocation possible.  box_f64 carries GC in the effect
// table but emits pure bit arithmetic — never a safepoint.  make_env
// counts even with the inline fast path: its slow path is the
// canonical safepoint, and covering both arms is correct (the fast
// path's reload folds back to the store).
export function isSafepoint(inst: Inst): boolean {
    if (inst.targets && inst.targets.length > 0) return false;
    if (inst.op === "box_f64") return false;
    const info = opInfo(inst.op);
    if (info.terminator) return false;
    return (info.effects & (Effect.GC | Effect.CALL)) !== 0;
}

function spillable(v: Inst): boolean {
    if (v.type !== "any") return false; // raw f64/i1: not references
    if (v.op === "const") return false; // rematerializes from statics
    if (v.targets && v.targets.length > 0) return false; // invoke results stay pinned
    return true;
}

function usesOf(inst: Inst, fn: (v: Inst) => void): void {
    for (const o of inst.operands) fn(o);
    if (inst.targets) for (const t of inst.targets) for (const a of t.args) if (a) fn(a);
}

// the set of values live across at least one safepoint, or null when
// the function needs no gc-frame
export function computeSpilledValues(fn: Func): Set<Inst> | null {
    let anySafepoint = false;
    fn.forEachInst((inst) => {
        if (isSafepoint(inst)) anySafepoint = true;
    });
    if (!anySafepoint) return null;

    // backward liveness to fixpoint.  sets keyed by inst; block liveOut
    // maps kept in an array parallel to fn.blocks.
    const liveIn = new Map<object, Set<Inst>>();
    const liveOut = new Map<object, Set<Inst>>();
    for (const b of fn.blocks) {
        liveIn.set(b, new Set());
        liveOut.set(b, new Set());
    }

    let changed = true;
    while (changed) {
        changed = false;
        // reverse block order is a decent schedule for backward flow
        for (let bi = fn.blocks.length - 1; bi >= 0; bi--) {
            const b = fn.blocks[bi]!;
            const out = liveOut.get(b)!;
            const before = out.size;
            const term = b.terminator;
            if (term && term.targets) {
                for (const t of term.targets) {
                    const sIn = liveIn.get(t.block);
                    if (!sIn) continue;
                    for (const v of sIn) out.add(v);
                    // successor params are defs there, not live into us
                    for (const p of t.block.params) out.delete(p);
                }
            }
            if (out.size !== before) changed = true;

            const live = new Set(out);
            for (let i = b.insts.length - 1; i >= 0; i--) {
                const inst = b.insts[i]!;
                live.delete(inst);
                usesOf(inst, (v) => live.add(v));
            }
            const inSet = liveIn.get(b)!;
            const inBefore = inSet.size;
            for (const v of live) inSet.add(v);
            if (inSet.size !== inBefore) changed = true;
        }
    }

    // record: for each safepoint, everything live just after it
    const spilled = new Set<Inst>();
    for (const b of fn.blocks) {
        const live = new Set(liveOut.get(b)!);
        for (let i = b.insts.length - 1; i >= 0; i--) {
            const inst = b.insts[i]!;
            // `live` here = live-after-inst
            if (isSafepoint(inst)) {
                for (const v of live) if (v !== inst && spillable(v)) spilled.add(v);
            }
            live.delete(inst);
            usesOf(inst, (v) => live.add(v));
        }
    }
    return spilled.size > 0 ? spilled : null;
}
