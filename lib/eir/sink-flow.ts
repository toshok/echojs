/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// Flow-sensitive allocation sinking + partial-escape materialization
// (docs/sinking-plan.md, sinking-P3).
//
// Extends the flow-insensitive sinks in optimize.ts to object
// candidates WITH field writes, and to candidates with exactly one
// escaping use.  Two structural facts carry the design:
//
//   - propSet's write diamonds are twins: both arms store the same
//     source value, so the after-join tracked value of a written field
//     is just that value — field phis are needed only at REAL control
//     joins (if/else arms writing different values, loop headers).
//   - Folding a shape guard FALSE is unconditionally sound (the
//     sinking-P1 twin argument), independent of writes.  A written
//     candidate folds every guard false and resolves everything
//     through the generic arms; the memory ops then vanish entirely,
//     and the post-fixpoint rawJoin/guard-region passes recover raw
//     f64 flow on the values.  (Folding TRUE under writes would need
//     repr-invariance reasoning — a set_prop_atom storing a non-number
//     into an f64 field repr-transitions the runtime shape.)
//
// The pass is all-or-nothing per candidate, and PLANS before it
// mutates: guard folding, though sound, routes the object generic, so
// a fold-then-decline would pessimize a surviving allocation.  Only
// when every screen passes does the rewrite run:
//
//   1. fold each guard's branch to its false edge, sweep the dead arms;
//   2. Braun-rename each field over the folded CFG (boxed block params
//      minted at joins, trivial ones removed) and fold every read to
//      its reaching value;
//   3. (partial escape) materialize a fresh literal of the reaching
//      field values immediately before the single escape instruction
//      and substitute it there — the runtime re-derives the true shape
//      from the actual values, so tracked-write repr drift is
//      immaterial;
//   4. delete the writes and the allocation.
//
// Fail-closed screens, with the reasons recorded in sinking-plan.md:
// own-key target-less writes only (a key-adding [[Set]] walks the
// prototype chain); no computed reads; no catch blocks in the rename
// region (unwind edges never carry tracked values); no use reachable
// from the escape (a later read would miss mutations through the
// alias); the escape executes at most once per allocation (a forward
// walk from the escape that finds any use — itself included — without
// first re-entering the allocation's block declines); the escape
// instruction plays no second role (a `o.self = o` write-escape
// declines).
//
// -fno-flow-sink bisects this pass alone.

import { Block, Func, Inst, Module, replaceAllUses } from "./ir";
// type-only imports: a value import would make optimize <-> sink-flow a
// runtime module cycle
import type { OptStats, Use, UseMap } from "./optimize";
import { condBrToBr, sweepUnreachableBlocks } from "./optimize-guards";

// the driver's per-round use map, indexed by inst.id (see the
// allocation-churn note on scanRound in optimize.ts — this pass runs
// last in the round and does no scans of its own)
const NO_USES: Use[] = [];

function usesOf(map: UseMap, v: Inst): Use[] {
    return map[v.id] || NO_USES;
}

// the candidate's field universe: names in literal order.  Shaped
// allocations key off the module shape table; unshaped ones off
// imms.keys (duplicate keys collapse to the LAST position's value, the
// ownObjectValue rule).
interface FieldInfo {
    names: string[];
    // field name -> operand index holding its initial value
    initial: Map<string, number>;
}

function fieldsOf(m: Module | undefined, alloc: Inst): FieldInfo | null {
    if (alloc.op === "make_object_shaped") {
        if (!m) return null;
        const fields = m.shapes.get(alloc.imms.shape as string);
        if (!fields || fields.length !== alloc.operands.length) return null;
        const initial = new Map<string, number>();
        const names: string[] = [];
        fields.forEach((f, i) => {
            names.push(f.name);
            initial.set(f.name, i);
        });
        return { names, initial };
    }
    // make_object
    const keys = alloc.imms.keys as readonly string[];
    const initial = new Map<string, number>();
    const names: string[] = [];
    keys.forEach((k, i) => {
        if (!initial.has(k)) names.push(k);
        initial.set(k, i); // last definition wins
    });
    // a __proto__ key is a prototype set, not a field; not ours
    if (initial.has("__proto__")) return null;
    return { names, initial };
}

// rename-region size cap (the ctor-sink REGION_BLOCK_CAP precedent,
// but for a COST model rather than a cloning one): sinking spreads the
// object's field values across the whole alloc-to-use region as live
// SSA values, so a large region trades one heap object for many
// long-lived gc-frame slots — measured on the stage1 self-compile,
// where flow-sinking esprima's scanPunctuator token literal (a
// function-spanning region) doubled the conservative pin-scan cost of
// every minor GC during parses and nearly doubled compile wall time.
// Small regions (loop accumulators, builder tails) keep the win.
const FLOW_REGION_CAP = 32;

// the classified plan for one candidate; built without mutating
interface Plan {
    guards: Inst[]; // has_shape, sole consumer its block's cond_br
    reads: Inst[]; // reachable own-key get_prop_atom, target-less
    writes: Inst[]; // reachable own-key set_prop_atom, target-less
    escape: Inst | null; // the single escape instruction, if any
    reachable: Set<Block>; // under folded guard branches
}

// successors under the fold plan: a cond_br whose condition is one of
// the candidate's guards takes only its false edge
function foldedSuccs(b: Block, guardSet: Set<Inst>): Block[] {
    const t = b.terminator;
    if (!t || !t.targets) return [];
    if (t.op === "cond_br" && guardSet.has(t.operands[0]!)) return [t.targets[1]!.block];
    return t.targets.map((tg) => tg.block);
}

function computeFoldedReachable(fn: Func, guardSet: Set<Inst>): Set<Block> {
    const reach = new Set<Block>([fn.entry!]);
    const wl: Block[] = [fn.entry!];
    while (wl.length > 0) {
        const b = wl.pop()!;
        for (const s of foldedSuccs(b, guardSet)) {
            if (!reach.has(s)) {
                reach.add(s);
                wl.push(s);
            }
        }
    }
    return reach;
}

// classify + screen one candidate; null = decline (nothing mutated)
function planOne(useMap: UseMap, fields: FieldInfo, alloc: Inst, uses: Use[]): Plan | null {
    const guards: Inst[] = [];
    const guardSet = new Set<Inst>();
    const reads: Inst[] = [];
    const writes: Inst[] = [];
    const slotOps: Inst[] = [];
    const escapes = new Map<Inst, boolean>(); // inst -> true (dedup multi-operand escapes)
    const roles = new Map<Inst, number>(); // 1=read/write, 2=escape (bitmask)
    const fn = alloc.block!.fn;

    for (const use of uses) {
        const { inst, index } = use;
        if (inst.block === null) continue; // already removed elsewhere this round
        if (index === -1) {
            escapes.set(inst, true);
            roles.set(inst, (roles.get(inst) ?? 0) | 2);
        } else if (inst.op === "has_shape" && index === 0) {
            // foldable only when its sole consumer is its block's cond_br
            const guardUses = usesOf(useMap, inst);
            if (
                guardUses.length === 1 &&
                guardUses[0]!.inst.op === "cond_br" &&
                guardUses[0]!.index === 0 &&
                guardUses[0]!.inst.block === inst.block
            ) {
                guards.push(inst);
                guardSet.add(inst);
            } else {
                return null; // unfoldable guard keeps the object alive
            }
        } else if (inst.op === "get_prop_atom" && index === 0) {
            if (inst.targets) return null;
            if (!fields.initial.has(inst.imms.atom as string)) return null; // prototype read
            reads.push(inst);
            roles.set(inst, (roles.get(inst) ?? 0) | 1);
        } else if (inst.op === "set_prop_atom" && index === 0) {
            if (inst.targets) return null;
            if (!fields.initial.has(inst.imms.atom as string)) return null; // key-adding write
            writes.push(inst);
            roles.set(inst, (roles.get(inst) ?? 0) | 1);
        } else if ((inst.op === "slot_load" || inst.op === "slot_store") && index === 0) {
            // these live in guarded fast arms; the fold must unreach them
            slotOps.push(inst);
        } else {
            escapes.set(inst, true);
            roles.set(inst, (roles.get(inst) ?? 0) | 2);
        }
    }

    // cheap pre-screen: pure-read candidates belong to the
    // flow-insensitive sinks — skip the CFG walks entirely
    if (writes.length === 0 && escapes.size === 0) return null;

    const reachable = computeFoldedReachable(fn, guardSet);
    if (!reachable.has(alloc.block!)) return null; // dead code: not ours to judge

    // every slot op must die with its arm; a reachable one means the
    // guard structure is not the lowering's (hand-built IR): decline
    for (const s of slotOps) if (reachable.has(s.block!)) return null;

    const liveReads = reads.filter((r) => reachable.has(r.block!));
    const liveWrites = writes.filter((w) => reachable.has(w.block!));
    const liveEscapes = [...escapes.keys()].filter((e) => reachable.has(e.block!));

    if (liveEscapes.length > 1) return null;
    const escape = liveEscapes.length === 1 ? liveEscapes[0]! : null;
    // the escape instruction must play no second role
    if (escape && (roles.get(escape)! & 1) !== 0) return null;

    // this pass exists for writes and escapes; pure read candidates
    // belong to the flow-insensitive sinks
    if (liveWrites.length === 0 && !escape) return null;
    // materializing at the escape must gain something
    if (escape && liveWrites.length === 0 && liveReads.length === 0) return null;

    // no use may be reachable FROM the escape (post-escape reads would
    // miss mutations through the alias; re-reaching the escape itself
    // would split the object's identity).  Re-entering the allocation's
    // block starts a fresh activation and stops the walk.
    if (escape) {
        const useInsts = new Set<Inst>([...guards, ...liveReads, ...liveWrites, escape]);
        const eb = escape.block!;
        const after = eb.insts.slice(eb.insts.indexOf(escape) + 1);
        for (const i of after) if (useInsts.has(i)) return null;
        const wl = foldedSuccs(eb, guardSet).filter((s) => s !== alloc.block);
        const seen = new Set<Block>(wl);
        while (wl.length > 0) {
            const b = wl.pop()!;
            if (!reachable.has(b)) continue;
            for (const i of b.insts) if (useInsts.has(i)) return null;
            for (const s of foldedSuccs(b, guardSet)) {
                if (s === alloc.block || seen.has(s)) continue;
                seen.add(s);
                wl.push(s);
            }
        }
    }

    // the rename region: reachable blocks the backward walk from the
    // uses can visit, up to (and excluding past) the allocation's
    // block.  No catch blocks — an unwind edge can't carry a tracked
    // value into a minted param.
    const useBlocks = new Set<Block>();
    for (const i of [...liveReads, ...liveWrites]) useBlocks.add(i.block!);
    if (escape) useBlocks.add(escape.block!);
    const region = new Set<Block>(useBlocks);
    const wl = [...useBlocks];
    while (wl.length > 0) {
        const b = wl.pop()!;
        if (b === alloc.block) continue;
        for (const e of b.predEdges) {
            const p = e.inst.block!;
            if (!reachable.has(p) || region.has(p)) continue;
            region.add(p);
            wl.push(p);
        }
    }
    for (const b of region) if (b.isCatch) return null;
    if (region.size > FLOW_REGION_CAP) return null;

    return { guards, reads: liveReads, writes: liveWrites, escape, reachable };
}

// --- the rewrite -----------------------------------------------------------

// Braun-style per-field renaming over the (already folded and swept)
// CFG.  Values are boxed SSA values; params minted at joins are boxed
// "any" params, verifier-legal on every edge.
class FieldRenamer {
    private fn: Func;
    private alloc: Inst;
    private fields: FieldInfo;
    // per block: candidate writes, relative order preserved
    private writesIn = new Map<Block, Inst[]>();
    // field -> block -> value at block ENTRY (params minted here; this
    // is the cycle-breaker memo, deliberately separate from the write
    // scan so a block that both joins and writes resolves reads before
    // its write to the entry value and reads after it to the write's)
    private entryMemo = new Map<string, Map<Block, Inst>>();
    // trivial-param forwarding chain.  A recursion frame can capture a
    // param that a NESTED cascade then removes — its replaceAllUses
    // runs before the outer frame installs the stale capture — so
    // every install point resolves through this map first.
    private forwarded = new Map<Inst, Inst>();

    resolve(v: Inst): Inst {
        for (;;) {
            const n = this.forwarded.get(v);
            if (!n) return v;
            v = n;
        }
    }

    constructor(fn: Func, alloc: Inst, fields: FieldInfo, writes: Inst[]) {
        this.fn = fn;
        this.alloc = alloc;
        this.fields = fields;
        for (const w of writes) {
            const b = w.block!;
            let list = this.writesIn.get(b);
            if (!list) this.writesIn.set(b, (list = []));
            list.push(w);
        }
        for (const list of this.writesIn.values())
            list.sort((a, b) => a.block!.insts.indexOf(a) - b.block!.insts.indexOf(b));
    }

    private memoFor(field: string): Map<Block, Inst> {
        let m = this.entryMemo.get(field);
        if (!m) this.entryMemo.set(field, (m = new Map()));
        return m;
    }

    // the reaching value at a program point: before insts[uptoIndex] of
    // `block` (uptoIndex past the end = block exit)
    valueAt(field: string, block: Block, uptoIndex: number): Inst {
        const list = this.writesIn.get(block);
        if (list) {
            for (let i = list.length - 1; i >= 0; i--) {
                const w = list[i]!;
                if ((w.imms.atom as string) !== field) continue;
                const wi = block.insts.indexOf(w);
                if (wi >= 0 && wi < uptoIndex) return w.operands[1]!;
            }
        }
        if (block === this.alloc.block) {
            const ai = block.insts.indexOf(this.alloc);
            if (ai >= 0 && ai < uptoIndex)
                return this.alloc.operands[this.fields.initial.get(field)!]!;
        }
        return this.valueAtEntry(field, block);
    }

    private valueAtEnd(field: string, block: Block): Inst {
        return this.valueAt(field, block, block.insts.length);
    }

    private valueAtEntry(field: string, block: Block): Inst {
        const memo = this.memoFor(field);
        const hit = memo.get(block);
        if (hit) return this.resolve(hit);

        const preds = block.predEdges;
        if (preds.length === 1) {
            const v = this.resolve(this.valueAtEnd(field, preds[0]!.inst.block!));
            memo.set(block, v);
            return v;
        }

        // join: mint a boxed param, memoized BEFORE recursing so loop
        // back-edges resolve to it.  argIndexOfParam is recomputed per
        // edge — a trivial-param removal during the recursion can
        // renumber this block's params — and the dependent-recheck
        // cascade can forward THIS param mid-fill, in which case the
        // memo already holds its replacement.
        const param = block.addParam("sink_" + field);
        memo.set(block, param);
        for (const e of preds) {
            const v = this.resolve(this.valueAtEnd(field, e.inst.block!));
            if (param.removed) break;
            e.inst.targets![e.targetIndex]!.args[block.argIndexOfParam(param)] = v;
        }
        if (param.removed) return this.resolve(memo.get(block)!);
        return this.tryRemoveTrivialParam(param);
    }

    // the builder's trivial-param rule: a param whose incoming
    // arguments are all the same value (or itself) forwards that value.
    // Dependent sink params (which may use this one as an edge
    // argument) are rechecked after the forward.
    private tryRemoveTrivialParam(param: Inst): Inst {
        if (param.removed) return param;
        const block = param.block!;
        const argIdx = block.argIndexOfParam(param);
        let same: Inst | null = null;
        for (const e of block.predEdges) {
            const arg = e.inst.targets![e.targetIndex]!.args[argIdx];
            // an unfilled slot means the param is mid-fill higher up
            // the recursion — never judge it yet
            if (!arg) return param;
            if (arg === same || arg === param) continue;
            if (same !== null) return param;
            same = arg;
        }
        if (same === null) return param;
        same = this.resolve(same);

        replaceAllUses(this.fn, param, same);
        this.forwarded.set(param, same);
        const dependents: Inst[] = [];
        for (const m of this.entryMemo.values())
            for (const [b, v] of m.entries())
                if (v === param) {
                    m.set(b, same);
                } else if (v.op === "blockparam" && !v.removed && v !== param) {
                    dependents.push(v);
                }
        block.removeParam(param);
        for (const d of dependents) if (!d.removed) this.tryRemoveTrivialParam(d);
        return same;
    }
}

function removeFromBlock(inst: Inst): void {
    const b = inst.block!;
    const idx = b.insts.indexOf(inst);
    if (idx >= 0) b.insts.splice(idx, 1);
    inst.block = null;
}

function applyPlan(
    fn: Func,
    fields: FieldInfo,
    alloc: Inst,
    plan: Plan,
    stats: OptStats
): void {
    // 1. fold the guard branches false and reclaim the dead arms (this
    //    disconnects every slot op and the fast-arm halves of the
    //    read/write diamonds; predEdges stay consistent for the renamer)
    for (const g of plan.guards) {
        const block = g.block!;
        const cbr = block.terminator;
        if (!cbr || cbr.op !== "cond_br" || cbr.operands[0] !== g) continue;
        condBrToBr(fn, block, 1);
        stats.shape_guards_sunk++;
    }
    sweepUnreachableBlocks(fn);

    const renamer = new FieldRenamer(fn, alloc, fields, plan.writes);

    // 2. fold every read to its reaching value
    for (const read of plan.reads) {
        if (!read.block) continue; // swept
        const v = renamer.resolve(
            renamer.valueAt(read.imms.atom as string, read.block, read.block.insts.indexOf(read))
        );
        replaceAllUses(fn, read, v);
        removeFromBlock(read);
        stats.reads_folded++;
    }

    // 3. materialize at the single escape, if any
    if (plan.escape && plan.escape.block) {
        const e = plan.escape;
        const eb = e.block!;
        const at = eb.insts.indexOf(e);
        // resolve AFTER all valueAt calls: a later field's renaming can
        // forward a param an earlier field's value captured
        const values = (
            alloc.op === "make_object_shaped"
                ? fields.names
                : (alloc.imms.keys as readonly string[])
        )
            .map((k) => renamer.valueAt(k, eb, at))
            .map((v) => renamer.resolve(v));
        const made =
            alloc.op === "make_object_shaped"
                ? new Inst(fn, "make_object_shaped", values, { shape: alloc.imms.shape })
                : new Inst(fn, "make_object", values, { keys: alloc.imms.keys });
        made.block = eb;
        eb.insts.splice(at, 0, made);
        for (let i = 0; i < e.operands.length; i++) if (e.operands[i] === alloc) e.operands[i] = made;
        if (e.targets)
            for (const t of e.targets)
                for (let i = 0; i < t.args.length; i++) if (t.args[i] === alloc) t.args[i] = made;
        stats.allocs_materialized++;
    }

    // 4. the writes and the allocation go
    for (const w of plan.writes) if (w.block) removeFromBlock(w);
    removeFromBlock(alloc);
    stats.flow_allocs_sunk++;
}

// try to flow-sink candidates in `fn`; at most ONE rewrite per call
// (the rewrite reshapes the CFG, so later candidates re-plan against
// fresh state on the driver's next fixpoint round).  The bisect flag
// (-fno-flow-sink) is read by the driver, not here (SinkFlags note),
// and the use map + candidate lists come from the driver's single
// per-round scan — this pass MUTATES without maintaining the map, so
// it must stay the round's last consumer.  Returns whether anything
// changed.
export function sinkFlowAllocations(
    fn: Func,
    m: Module | undefined,
    stats: OptStats,
    useMap: UseMap,
    objCandidates: Inst[],
    shapedCandidates: Inst[]
): boolean {
    const tryOne = (alloc: Inst): boolean => {
        if (!alloc.block) return false;
        const fields = fieldsOf(m, alloc);
        if (!fields) return false;
        const plan = planOne(useMap, fields, alloc, usesOf(useMap, alloc));
        if (!plan) return false;
        applyPlan(fn, fields, alloc, plan, stats);
        return true;
    };
    for (const alloc of objCandidates) {
        if (alloc.op !== "make_object") continue; // make_array: not ours
        if (tryOne(alloc)) return true;
    }
    if (m) for (const alloc of shapedCandidates) if (tryOne(alloc)) return true;
    return false;
}
