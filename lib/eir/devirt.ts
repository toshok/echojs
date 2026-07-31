/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// direct-call devirtualization beyond the self binding.
// Lowering only marks direct calls for self-recursion; module
// functions call each other through their %self slots, and function
// expressions through their SSA closure values.  When the CALLEE
// IDENTITY of a plain `call` is provable, the call skips closure
// dispatch (_ejs_invoke_closure) and calls the EIR function directly —
// same calling convention, argc/argv passed as before, so callee-side
// defaults/rest/arguments all still work.
//
// Two provable shapes:
//   - SSA-visible: the callee operand IS a make_closure in the same
//     function.  The direct call's env operand is the closure's env.
//   - stable-slot: the callee operand is a module_slot_load of a %self
//     slot with exactly ONE static store in the module, whose stored
//     value is a make_closure, and the load provably observes the
//     store: either the store sits in the toplevel entry block with no
//     CALL-effect instruction before it (no user code can run before
//     the slot is written — cross-function safe, mirroring
//     specialize.ts's prefix rule), or the store dominates the load in
//     the same function.  Cross-function sites can't carry the env
//     value, so they additionally require the callee's %env param to
//     be entirely unused (typical for module-level functions — their
//     free names resolve through module slots, not the environment).
//
// What invoke_closure does that a direct call skips: the IS_FUNCTION
// check (statically true — the value is this closure) and the
// class-constructor TypeError.  The latter is why any function whose
// closure might flow into set_constructor_kind_base/derived is
// declined; when that flow isn't enumerable (the marking intrinsic's
// operand is neither a make_closure nor a %self slot load), the pass
// declines the whole module (fail closed).
//
// Runs AFTER specialization (integrate.ts): a devirtualized site no
// longer uses its closure/slot-load as a plain-call callee, which
// would otherwise make specialize.ts's closed-world enumeration
// decline the strictly-better call_typed rewrite.

import { Module, Func, Inst, Block } from "./ir";
import { Effect, opInfo } from "./ops";
import { computeRPO, computeDominators, dominates } from "./verifier";
import { passes } from "../pass-config";

export interface DevirtStats {
    // call sites rewritten against an SSA-visible make_closure
    ssa_sites: number;
    // call sites rewritten through a stable %self slot
    slot_sites: number;
}

function comesBefore(idom: Map<Block, Block>, a: Inst, b: Inst): boolean {
    const ba = a.block!;
    const bb = b.block!;
    if (ba === bb) return ba.insts.indexOf(a) < bb.insts.indexOf(b);
    return dominates(idom, ba, bb);
}

export function devirtualizeModule(m: Module, toplevelName: string): DevirtStats {
    const stats: DevirtStats = { ssa_sites: 0, slot_sites: 0 };
    if (!passes().devirt) return stats;

    const fnByName = new Map<string, Func>();
    for (const fn of m.functions) fnByName.set(fn.name, fn);
    const toplevelFn = fnByName.get(toplevelName);

    // --- class-constructor suspects (fail closed) ---------------------------
    // a devirtualized call to a class constructor would skip
    // invoke_closure's TypeError; enumerate every closure the marking
    // intrinsic could reach and decline those functions.
    const ctorSuspect = new Set<string>();
    const suspectSlots = new Set<number>();
    let bailAll = false;
    for (const fn of m.functions) {
        fn.forEachInst((inst) => {
            if (inst.op !== "call_runtime") return;
            const name = inst.imms["name"] as string;
            if (typeof name !== "string" || name.indexOf("set_constructor_kind") !== 0) return;
            const v = inst.operands[0];
            if (!v) return;
            if (v.op === "make_closure") ctorSuspect.add(v.imms["fn"] as string);
            else if (v.op === "module_slot_load" && v.imms["module"] === "%self")
                suspectSlots.add(v.imms["slot"] as number);
            else bailAll = true;
        });
    }
    if (bailAll) return stats;

    // --- %self slot stores --------------------------------------------------
    const selfStores = new Map<number, { fn: Func; inst: Inst }[]>();
    for (const fn of m.functions) {
        fn.forEachInst((inst) => {
            if (inst.op !== "module_slot_store" || inst.imms["module"] !== "%self") return;
            const slot = inst.imms["slot"] as number;
            let l = selfStores.get(slot);
            if (!l) selfStores.set(slot, (l = []));
            l.push({ fn, inst });
        });
    }
    // anything stored to a ctor-marked slot is suspect too; a
    // non-closure store to one means we can't enumerate — fail closed
    suspectSlots.forEach((slot) => {
        for (const s of selfStores.get(slot) || []) {
            if (s.inst.operands[0]!.op === "make_closure")
                ctorSuspect.add(s.inst.operands[0]!.imms["fn"] as string);
            else bailAll = true;
        }
    });
    if (bailAll) return stats;

    // --- helpers ------------------------------------------------------------
    const envUnusedCache = new Map<Func, boolean>();
    const envUnused = (fn: Func): boolean => {
        let r = envUnusedCache.get(fn);
        if (r !== undefined) return r;
        const envParam = fn.entry ? fn.entry.params[0] : undefined;
        r = true;
        if (envParam) {
            fn.forEachInst((inst) => {
                for (const o of inst.operands) if (o === envParam) r = false;
                if (inst.targets)
                    for (const t of inst.targets)
                        for (const a of t.args) if (a === envParam) r = false;
            });
        }
        envUnusedCache.set(fn, r);
        return r;
    };

    // the store sits in the toplevel entry with no CALL-effect
    // instruction before it: no user code can observe the slot's
    // pre-store state (except a textually-earlier load in that same
    // entry block — the documented hoisting-lost read)
    const prefixSafeCache = new Map<Inst, boolean>();
    const prefixSafe = (store: Inst, storeFn: Func): boolean => {
        let r = prefixSafeCache.get(store);
        if (r !== undefined) return r;
        r = false;
        if (toplevelFn && storeFn === toplevelFn && store.block === toplevelFn.entry) {
            r = true;
            for (const inst of toplevelFn.entry!.insts) {
                if (inst === store) break;
                if ((opInfo(inst.op).effects & Effect.CALL) !== 0) {
                    r = false;
                    break;
                }
            }
        }
        prefixSafeCache.set(store, r);
        return r;
    };

    const idoms = new Map<Func, Map<Block, Block>>();
    const idomOf = (fn: Func): Map<Block, Block> => {
        let d = idoms.get(fn);
        if (!d) {
            const { rpo } = computeRPO(fn);
            idoms.set(fn, (d = computeDominators(fn, rpo)));
        }
        return d;
    };

    // --- the rewrite --------------------------------------------------------
    // candidates first (rewriting inserts instructions, which must not
    // happen under forEachInst's live iteration)
    for (const fn of m.functions) {
        const ssa: { call: Inst; closure: Inst; name: string }[] = [];
        const slot: { call: Inst; name: string }[] = [];
        fn.forEachInst((inst) => {
            if (inst.op !== "call" || inst.imms["direct"]) return;
            const callee = inst.operands[0]!;

            if (callee.op === "make_closure") {
                const name = callee.imms["fn"] as string;
                if (ctorSuspect.has(name) || !fnByName.has(name)) return;
                ssa.push({ call: inst, closure: callee, name });
                return;
            }

            if (callee.op === "module_slot_load" && callee.imms["module"] === "%self") {
                const slotNum = callee.imms["slot"] as number;
                const stores = selfStores.get(slotNum) || [];
                if (stores.length !== 1) return;
                const { fn: storeFn, inst: store } = stores[0]!;
                const closure = store.operands[0]!;
                if (closure.op !== "make_closure") return;
                const name = closure.imms["fn"] as string;
                const target = fnByName.get(name);
                if (!target || ctorSuspect.has(name)) return;
                if (!envUnused(target)) return;
                // the load must provably observe the store
                const load = callee;
                let orderOk: boolean;
                if (prefixSafe(store, storeFn)) {
                    orderOk = !(
                        load.block === store.block &&
                        store.block!.insts.indexOf(load) < store.block!.insts.indexOf(store)
                    );
                } else {
                    orderOk = storeFn === fn && comesBefore(idomOf(fn), store, load);
                }
                if (!orderOk) return;
                slot.push({ call: inst, name });
            }
        });
        for (const c of ssa) {
            c.call.imms["direct"] = c.name;
            c.call.operands[0] = c.closure.operands[0]!;
            stats.ssa_sites++;
        }
        for (const c of slot) {
            const env = new Inst(fn, "const", [], { kind: "undefined" });
            const b = c.call.block!;
            env.block = b;
            b.insts.splice(b.insts.indexOf(c.call), 0, env);
            c.call.imms["direct"] = c.name;
            c.call.operands[0] = env;
            stats.slot_sites++;
        }
    }
    return stats;
}
