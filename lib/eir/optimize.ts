/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// EIR optimization passes.  The first: allocation sinking (scalar
// replacement) for non-escaping object/array literals, plus the dead
// pure-instruction elimination that sweeps up after it.
//
// The effect table in ops.ts is the contract here: nothing below
// pattern-matches behavior that isn't declared there, with one narrow
// exception — the alloc ops' WRITE effect covers writes to their own
// fresh storage, so a dead allocation is removable even though a dead
// WRITE-effect instruction generally isn't.
//
// Everything is intra-function and flow-insensitive.  An allocation is
// sinkable only if every use is a base-position property get/set; a read
// folds only if its key is an own data property that is never written
// (so the initial value flows everywhere without any CFG reasoning —
// non-escape means no one else can write it).  Instructions carrying
// explicit normal/unwind targets (may-throw ops inside protected
// regions) are block terminators; we neither fold nor remove them.

import { Func, Inst, Module, ShapeField, replaceAllUses } from "./ir";
import { Effect, opInfo } from "./ops";
import {
    condBrToBr,
    optimizeGuardRegions,
    optimizeShapeRegions,
    rawJoinParams,
    sweepUnreachableBlocks,
    threadBooleanJoins,
} from "./optimize-guards";

export interface OptStats {
    allocs_sunk: number;
    reads_folded: number;
    calls_inlined: number;
    iters_folded: number;
    dead_removed: number;
    // Phase 3.4 guard-region passes (optimize-guards.ts)
    guards_folded: number;
    regions_merged: number;
    raw_join_params: number;
    // shapes-plan P4.3: shape-guard region passes
    shape_guards_folded: number;
    shape_regions_merged: number;
    // shapes-plan P4.5: heterogeneous (shape + numeric) region merges
    shape_numeric_merged: number;
    // Phase 3.6: unbox_f64(box_f64(x)) round-trips annihilated
    unbox_folds: number;
    // Phase 3.6: constant edges threaded past boxed-boolean re-tests
    joins_threaded: number;
    // sinking-plan S1: non-escaping make_object_shaped scalar-replaced,
    // and the shape guards on them resolved statically
    shape_allocs_sunk: number;
    shape_guards_sunk: number;
}

function newStats(): OptStats {
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
    };
}

// uses of `value` within fn, with enough position info to classify
interface Use {
    inst: Inst;
    // operand index, or -1 for a branch-edge argument
    index: number;
}

// one full-function scan per fixpoint round, shared by every pass in
// the round; the mutation helpers below keep it accurate.  storage is a
// plain array indexed by inst.id (dense per-function) — this code runs
// under the echojs runtime during self-compiles, where Map traffic and
// allocation churn are far more expensive than under V8.
const EMPTY_USES: Use[] = [];

type UseMap = (Use[] | undefined)[];

function buildUseMap(fn: Func): UseMap {
    const map: UseMap = new Array(fn.next_value_id);
    const add = (v: Inst, inst: Inst, index: number) => {
        const list = map[v.id];
        if (list) list.push({ inst, index });
        else map[v.id] = [{ inst, index }];
    };
    fn.forEachInst((inst) => {
        for (let i = 0; i < inst.operands.length; i++) add(inst.operands[i]!, inst, i);
        if (inst.targets) {
            for (const t of inst.targets) for (const a of t.args) if (a) add(a, inst, -1);
        }
    });
    return map;
}

function usesOf(uses: UseMap, value: Inst): Use[] {
    return uses[value.id] || EMPTY_USES;
}

function removeInst(uses: UseMap, inst: Inst): void {
    const b = inst.block!;
    const idx = b.insts.indexOf(inst);
    if (idx >= 0) b.insts.splice(idx, 1);
    inst.block = null;
    // inst no longer uses its operands
    for (const o of inst.operands) {
        const list = uses[o.id];
        if (list) uses[o.id] = list.filter((u) => u.inst !== inst);
    }
    uses[inst.id] = undefined;
}

// --- allocation sinking ----------------------------------------------------

// how an allocation's use participates, per classifyUses
interface AllocUses {
    // get_prop_atom reads, by atom
    atomReads: Inst[];
    // set_prop_atom writes (alloc in base position only)
    atomWrites: Inst[];
    // get_prop reads with the alloc as base
    computedReads: Inst[];
    escapes: boolean;
}

// classify every use of a make_object/make_array result.  base-position
// gets and atom-keyed sets are the only non-escaping uses; anything else
// (call/return/throw operands, edge arguments, value or key positions,
// computed sets — whose key evaluation we must not disturb — accessor
// defines, deletes) escapes.
function classifyUses(uses: UseMap, alloc: Inst): AllocUses {
    const r: AllocUses = { atomReads: [], atomWrites: [], computedReads: [], escapes: false };
    for (const use of usesOf(uses, alloc)) {
        const { inst, index } = use;
        if (index === -1) {
            r.escapes = true; // flows into a block param
        } else if (inst.op === "get_prop_atom" && index === 0) {
            r.atomReads.push(inst);
        } else if (inst.op === "set_prop_atom" && index === 0) {
            r.atomWrites.push(inst);
        } else if (inst.op === "get_prop" && index === 0) {
            r.computedReads.push(inst);
        } else {
            r.escapes = true;
        }
    }
    return r;
}

// the own-key initial value for `atom` in a make_object, honoring
// duplicate keys (last definition wins)
function ownObjectValue(alloc: Inst, atom: string): Inst | null {
    const keys = alloc.imms.keys as readonly string[];
    for (let i = keys.length - 1; i >= 0; i--) {
        if (keys[i] === atom) return alloc.operands[i]!;
    }
    return null;
}

// the element initial value for a const-numeric index into a make_array,
// or null for holes / out-of-range / non-element keys
function ownArrayElement(alloc: Inst, index: number): Inst | null {
    if (!Number.isInteger(index) || index < 0) return null;
    if (alloc.imms.len === undefined) {
        // dense: operands are the elements in order
        return index < alloc.operands.length ? alloc.operands[index]! : null;
    }
    // holey: imms.indices[i] is the array index operand i lands at
    const indices = alloc.imms.indices as readonly number[];
    for (let i = 0; i < indices.length; i++) {
        if (indices[i] === index) return alloc.operands[i]!;
    }
    return null;
}

function arrayLength(alloc: Inst): number {
    return alloc.imms.len !== undefined ? (alloc.imms.len as number) : alloc.operands.length;
}

// fold a read to `value`: all the read's uses see the value directly,
// and the read disappears.  only for target-less reads — a read with
// unwind targets terminates its block and can't simply vanish.
function foldRead(uses: UseMap, fn: Func, read: Inst, value: Inst): void {
    replaceAllUses(fn, read, value);
    const inherited = uses[read.id];
    if (inherited && inherited.length > 0) {
        const list = uses[value.id];
        if (list) list.push(...inherited);
        else uses[value.id] = inherited.slice();
    }
    uses[read.id] = undefined;
    removeInst(uses, read);
}

// materialize a `const` number in front of `before` (for .length folds)
function constNumberBefore(fn: Func, before: Inst, value: number): Inst {
    const c = new Inst(fn, "const", [], { kind: "number", value: value });
    const b = before.block!;
    c.block = b;
    b.insts.splice(b.insts.indexOf(before), 0, c);
    return c;
}

// try to scalar-replace one allocation.  returns true if anything changed.
function sinkAlloc(useMap: UseMap, fn: Func, alloc: Inst, stats: OptStats): boolean {
    const isArray = alloc.op === "make_array";
    const uses = classifyUses(useMap, alloc);
    if (uses.escapes) return false;

    let changed = false;
    const writtenAtoms = new Set<string>();
    for (const w of uses.atomWrites) writtenAtoms.add(w.imms.atom as string);

    if (isArray) {
        // element writes can't reach make_array (set_prop is an escape),
        // but a `length` write truncates — it blocks every fold
        if (writtenAtoms.size === 0) {
            for (const read of uses.atomReads) {
                if (read.targets) continue;
                if ((read.imms.atom as string) !== "length") continue; // prototype read
                foldRead(useMap, fn, read, constNumberBefore(fn, read, arrayLength(alloc)));
                stats.reads_folded++;
                changed = true;
            }
            for (const read of uses.computedReads) {
                if (read.targets) continue;
                const key = read.operands[1]!;
                if (key.op !== "const" || key.imms.kind !== "number") continue;
                const el = ownArrayElement(alloc, key.imms.value as number);
                if (!el) continue; // hole or out of range: prototype read
                foldRead(useMap, fn, read, el);
                stats.reads_folded++;
                changed = true;
            }
        }
    } else {
        for (const read of uses.atomReads) {
            if (read.targets) continue;
            const atom = read.imms.atom as string;
            if (writtenAtoms.has(atom)) continue; // flow-sensitive: not yet
            const v = ownObjectValue(alloc, atom);
            if (!v) continue; // not an own key: prototype read
            foldRead(useMap, fn, read, v);
            stats.reads_folded++;
            changed = true;
        }
    }

    // if only atom writes remain, the allocation is write-only and dies
    // along with its stores — but only stores to OWN keys are provably
    // unobservable ([[Set]] to a non-own key walks the prototype chain,
    // where a pathological accessor could intercept it).  arrays' one
    // own atom is `length`.
    const ownWrite = (w: Inst) =>
        isArray
            ? (w.imms.atom as string) === "length"
            : ownObjectValue(alloc, w.imms.atom as string) !== null;
    const remaining = classifyUses(useMap, alloc);
    if (
        !remaining.escapes &&
        remaining.atomReads.length === 0 &&
        remaining.computedReads.length === 0 &&
        remaining.atomWrites.every((w) => !w.targets && ownWrite(w))
    ) {
        for (const w of remaining.atomWrites) removeInst(useMap, w);
        removeInst(useMap, alloc);
        stats.allocs_sunk++;
        changed = true;
    }
    return changed;
}

// --- shaped-literal sinking (sinking-plan S1) ------------------------------
//
// make_object_shaped carries its field values as operands (shape field
// order, boxed) and its shape as an immediate — there are no
// initializing stores.  For a non-escaping, never-written shaped
// allocation the birth shape is invariant for the object's whole
// lifetime (nothing else can transition it), so its has_shape guards
// resolve statically and its slot/get reads fold to the operands.
// Guards fold TRUE only when every f64-repr field's operand is provably
// a number (box_f64 or number const) — folding true exposes raw
// slot_loads, and feeding those from a non-number would manufacture
// garbage bits.  Folding FALSE is always sound: the diamond's arms are
// twins, and the generic arm's reads fold to the same operands.

// how a shaped allocation's use participates
interface ShapedAllocUses {
    // has_shape guards whose result feeds only their block's cond_br
    guards: Inst[];
    // slot_load reads against the birth shape
    slotReads: Inst[];
    // get_prop_atom reads (own-field ones fold; others block removal)
    atomReads: Inst[];
    // has_shape whose result ALSO flows somewhere else — leaves the
    // alloc alive but doesn't escape it
    unfoldableGuards: Inst[];
    escapes: boolean;
}

function classifyShapedUses(useMap: UseMap, alloc: Inst): ShapedAllocUses {
    const r: ShapedAllocUses = {
        guards: [],
        slotReads: [],
        atomReads: [],
        unfoldableGuards: [],
        escapes: false,
    };
    for (const use of usesOf(useMap, alloc)) {
        const { inst, index } = use;
        if (index === -1) {
            r.escapes = true;
        } else if (inst.op === "has_shape" && index === 0) {
            const guardUses = usesOf(useMap, inst);
            if (
                guardUses.length === 1 &&
                guardUses[0]!.inst.op === "cond_br" &&
                guardUses[0]!.index === 0 &&
                guardUses[0]!.inst.block === inst.block
            )
                r.guards.push(inst);
            else r.unfoldableGuards.push(inst);
        } else if (inst.op === "slot_load" && index === 0) {
            r.slotReads.push(inst);
        } else if (inst.op === "get_prop_atom" && index === 0) {
            r.atomReads.push(inst);
        } else {
            // every write (slot_store, set_prop_atom), computed access,
            // call/return/throw operand, value position: escape.  v1
            // keeps writes out entirely — a write would also invalidate
            // the static guard resolution above.
            r.escapes = true;
        }
    }
    return r;
}

// is this operand provably a number (safe to feed a raw f64 slot)?
function provablyNumberOperand(v: Inst): boolean {
    return v.op === "box_f64" || (v.op === "const" && v.imms.kind === "number");
}

// the raw-f64 replacement for a slot_load of field value `v`, inserted
// before `read` when a fresh const is needed
function rawF64ValueBefore(fn: Func, read: Inst, v: Inst): Inst | null {
    if (v.op === "box_f64") return v.operands[0]!;
    if (v.op === "const" && v.imms.kind === "number") {
        const c = new Inst(fn, "f64_const", [], { value: v.imms.value });
        c.type = "f64";
        const b = read.block!;
        c.block = b;
        b.insts.splice(b.insts.indexOf(read), 0, c);
        return c;
    }
    return null;
}

function shapedFieldIndex(fields: readonly ShapeField[], name: string): number {
    for (let i = 0; i < fields.length; i++) if (fields[i]!.name === name) return i;
    return -1;
}

// try to scalar-replace one shaped allocation.  reads fold immediately;
// guard branches rewrite to their resolved edge (the dead arm and the
// then-unused has_shape are reclaimed by the caller's unreachable-block
// sweep + DCE, and the alloc itself is removed in a later round once
// its use list has drained).
function sinkShapedAlloc(
    useMap: UseMap,
    fn: Func,
    m: Module,
    alloc: Inst,
    stats: OptStats
): boolean {
    const shape = alloc.imms.shape as string;
    const fields = m.shapes.get(shape);
    if (!fields || fields.length !== alloc.operands.length) return false;

    const uses = classifyShapedUses(useMap, alloc);
    if (uses.escapes) return false;

    // guards fold true only when every f64 field's operand is provably
    // a number; otherwise the generic arm is the (equally correct) route
    const reprsProven = fields.every(
        (f, i) => f.repr !== "f64" || provablyNumberOperand(alloc.operands[i]!)
    );

    let changed = false;

    for (const read of uses.slotReads) {
        if (read.targets) continue;
        if ((read.imms.shape as string) !== shape) continue; // other-shape arm: dies with it
        const k = read.imms.slot as number;
        if (k < 0 || k >= fields.length) continue;
        const v = alloc.operands[k]!;
        if ((read.imms.repr as string) === "f64") {
            const raw = rawF64ValueBefore(fn, read, v);
            if (!raw) continue; // unprovable: the false-folded guard keeps this arm dead
            foldRead(useMap, fn, read, raw);
        } else {
            foldRead(useMap, fn, read, v);
        }
        stats.reads_folded++;
        changed = true;
    }

    for (const read of uses.atomReads) {
        if (read.targets) continue;
        const k = shapedFieldIndex(fields, read.imms.atom as string);
        if (k < 0) continue; // prototype read: unfoldable, blocks removal
        foldRead(useMap, fn, read, alloc.operands[k]!);
        stats.reads_folded++;
        changed = true;
    }

    for (const guard of uses.guards) {
        const block = guard.block!;
        const cbr = block.terminator!;
        if (cbr.op !== "cond_br") continue; // already rewritten this round
        const takeTrue = (guard.imms.shape as string) === shape && reprsProven;
        condBrToBr(fn, block, takeTrue ? 0 : 1);
        // the cond_br is gone; keep the round's use map accurate
        const guardUses = useMap[guard.id];
        if (guardUses) useMap[guard.id] = guardUses.filter((u) => u.inst !== cbr);
        stats.shape_guards_sunk++;
        changed = true;
    }

    // when nothing uses the alloc anymore, it goes now; otherwise the
    // next fixpoint round (fresh use map, dead arms swept) finishes
    const remaining = usesOf(useMap, alloc).filter((u) => u.inst.block !== null);
    if (remaining.length === 0) {
        removeInst(useMap, alloc);
        stats.shape_allocs_sunk++;
        changed = true;
    }
    return changed;
}

function sinkAllocations(useMap: UseMap, fn: Func, m: Module | undefined, stats: OptStats): boolean {
    const candidates: Inst[] = [];
    const shaped: Inst[] = [];
    const noShaped = !!process.env["EJS_NO_SHAPED_SINK"];
    fn.forEachInst((inst) => {
        if (inst.op === "make_object" || inst.op === "make_array") candidates.push(inst);
        else if (inst.op === "make_object_shaped" && !noShaped) shaped.push(inst);
    });
    let changed = false;
    for (const c of candidates) {
        if (!c.block) continue; // removed by an earlier candidate's fold
        if (sinkAlloc(useMap, fn, c, stats)) changed = true;
    }
    if (m) {
        for (const c of shaped) {
            if (!c.block) continue;
            if (sinkShapedAlloc(useMap, fn, m, c, stats)) changed = true;
        }
    }
    return changed;
}

// --- direct IIFE inlining ------------------------------------------------------

// the desugars (destructuring especially) wrap expression-position work
// in immediately-called closures: make_env / env_store / make_closure /
// call.  inlining the call is what exposes the env and the literals
// inside it to the sinking passes above.
//
// conservatively inlinable callee: a single block ending in `return`,
// no frame-dependent ops (arguments/rest/new.target/super), and an
// unused %this param (the IIFE arrows never touch it — lexical `this`
// rides in the env).  the call itself must carry no unwind targets.

const FRAME_OPS = new Set([
    "args_obj",
    "rest_args",
    "new_target",
    "construct_super",
    "construct_super_apply",
]);

const INLINE_MAX_INSTS = 40;

function inlinableCallee(m: Module, caller: Func, closure: Inst): Func | null {
    const name = closure.imms.fn as string;
    const callee = m.functions.find((f) => f.name === name);
    if (!callee || callee === caller) return null;
    if (callee.blocks.length !== 1) return null;
    const entry = callee.entry!;
    if (entry.insts.length > INLINE_MAX_INSTS) return null;
    const term = entry.terminator;
    if (!term || term.op !== "return") return null;
    for (const inst of entry.insts) {
        if (FRAME_OPS.has(inst.op)) return null;
        if (inst.targets && inst !== term) return null;
    }
    // %this must be unused (we'd otherwise have to reason about the
    // runtime's this-coercion on the call path we're deleting)
    const thisParam = entry.params[1];
    if (thisParam) {
        for (const inst of entry.insts) {
            for (const o of inst.operands) if (o === thisParam) return null;
        }
    }
    return callee;
}

function constUndefinedBefore(fn: Func, before: Inst): Inst {
    const c = new Inst(fn, "const", [], { kind: "undefined" });
    const b = before.block!;
    c.block = b;
    b.insts.splice(b.insts.indexOf(before), 0, c);
    return c;
}

// inline `call` (operands [closure, this, ...args]) by cloning the
// callee's single block in front of it
function inlineCall(fn: Func, call: Inst, closure: Inst, callee: Func): void {
    const entry = callee.entry!;
    const subst = new Map<Inst, Inst>();

    // params: [%env, %this, ...declared] -> [closure env, call this, args]
    for (let i = 0; i < entry.params.length; i++) {
        const p = entry.params[i]!;
        let v: Inst;
        if (i === 0) v = closure.operands[0]!;
        else if (i < call.operands.length) v = call.operands[i]!;
        else v = constUndefinedBefore(fn, call);
        subst.set(p, v);
    }

    const map = (v: Inst): Inst => subst.get(v) || v;
    const block = call.block!;
    let at = block.insts.indexOf(call);
    let result: Inst | null = null;
    for (const inst of entry.insts) {
        if (inst === entry.terminator) {
            result = map(inst.operands[0]!);
            break;
        }
        const clone = new Inst(fn, inst.op, inst.operands.map(map), { ...inst.imms });
        clone.block = block;
        block.insts.splice(at++, 0, clone);
        subst.set(inst, clone);
    }
    replaceAllUses(fn, call, result!);
    // no live use map here — inlineDirectCalls rebuilds nothing; the
    // subsequent passes each build their own
    const b = call.block!;
    const idx = b.insts.indexOf(call);
    if (idx >= 0) b.insts.splice(idx, 1);
    call.block = null;
}

function inlineDirectCalls(m: Module, fn: Func, stats: OptStats): boolean {
    const candidates: { call: Inst; closure: Inst; callee: Func }[] = [];
    fn.forEachInst((inst) => {
        if (inst.op !== "call" || inst.imms.direct || (inst.targets && inst.targets.length > 0))
            return;
        const closure = inst.operands[0]!;
        if (closure.op !== "make_closure" || closure.block === null) return;
        const callee = inlinableCallee(m, fn, closure);
        if (callee) candidates.push({ call: inst, closure, callee });
    });
    for (const c of candidates) {
        inlineCall(fn, c.call, c.closure, c.callee);
        stats.calls_inlined++;
    }
    return candidates.length > 0;
}

// --- env scalar replacement -----------------------------------------------------

// a make_env whose only uses are base-position env_load/env_store, all
// in the block that allocated it, resolves by a linear walk: each load
// sees the most recent store to its slot (or undefined — env slots
// start undefined, echojs has no TDZ).  parent-env chaining stores the
// env in a VALUE position, which classifies as an escape below.
function scalarReplaceEnvs(useMap: UseMap, fn: Func, stats: OptStats): boolean {
    let changed = false;
    const candidates: Inst[] = [];
    fn.forEachInst((inst) => {
        if (inst.op === "make_env") candidates.push(inst);
    });

    for (const env of candidates) {
        if (!env.block) continue;
        let ok = true;
        for (const use of usesOf(useMap, env)) {
            const { inst, index } = use;
            const local =
                index === 0 &&
                (inst.op === "env_load" || inst.op === "env_store") &&
                inst.block === env.block;
            if (!local) {
                ok = false;
                break;
            }
        }
        if (!ok) continue;

        // linear walk of the defining block.  replacements materialize
        // after the walk — inserting into insts mid-iteration would
        // shift the very array being walked.
        const slotValues = new Map<number, Inst>();
        const loads: [Inst, Inst | null][] = []; // load -> replacement (null = undefined)
        const stores: Inst[] = [];
        let started = false;
        for (const inst of env.block.insts) {
            if (inst === env) {
                started = true;
                continue;
            }
            if (!started || inst.operands[0] !== env) continue;
            if (inst.op === "env_store") {
                slotValues.set(inst.imms.slot as number, inst.operands[1]!);
                stores.push(inst);
            } else if (inst.op === "env_load") {
                loads.push([inst, slotValues.get(inst.imms.slot as number) || null]);
            }
        }
        for (const [load, v] of loads) foldRead(useMap, fn, load, v || constUndefinedBefore(fn, load));
        for (const s of stores) removeInst(useMap, s);
        removeInst(useMap, env);
        stats.allocs_sunk++;
        stats.reads_folded += loads.length;
        changed = true;
    }
    return changed;
}

// --- iterator-protocol peephole -------------------------------------------------

// array destructuring desugars to an iterator walk; over a dense array
// literal the whole chain is compile-time constant:
//
//     %a = make_array e0, e1, ...
//     %s = get_global atom="Symbol"
//     %i = get_prop_atom %s, atom="iterator"
//     %f = get_prop %a, %i
//     %t = call %f, %a
//     %w = call_runtime %t, name="iterator_wrapper_new"
//     %g = get_prop_atom %w, atom="getNextValue"
//     %v = call %g, %w                          ; k-th call = element k
//
// the k-th getNextValue call folds to the k-th element (undefined past
// the end — the array iterator yields undefined there).  the fold
// assumes the built-in Symbol global and Array.prototype[Symbol.iterator]
// (the desugar already bakes in the former by emitting get_global).
// dense literals only: a hole would read through the prototype chain.
//
// use discipline is strict — every link is consumed only by the next
// (a getRest, an extra array use, a cross-block call, or anything
// carrying unwind targets fails the match), so rest patterns and
// escaping arrays keep the runtime walk.
function foldIteratorWrappers(useMap: UseMap, fn: Func, stats: OptStats): boolean {
    let changed = false;
    const wrappers: Inst[] = [];
    fn.forEachInst((inst) => {
        if (inst.op === "call_runtime" && inst.imms.name === "iterator_wrapper_new")
            wrappers.push(inst);
    });

    const hasTargets = (i: Inst) => i.targets !== null && i.targets.length > 0;
    const soleUse = (v: Inst, user: Inst) => {
        const u = usesOf(useMap, v);
        return u.length === 1 && u[0]!.inst === user;
    };

    for (const w of wrappers) {
        if (!w.block || hasTargets(w)) continue;

        // match the creation chain backwards
        const it = w.operands[0]!;
        if (it.op !== "call" || it.operands.length !== 2 || it.imms.direct || hasTargets(it))
            continue;
        const itfn = it.operands[0]!;
        const arr = it.operands[1]!;
        if (itfn.op !== "get_prop" || itfn.operands[0] !== arr || hasTargets(itfn)) continue;
        const symprop = itfn.operands[1]!;
        if (symprop.op !== "get_prop_atom" || symprop.imms.atom !== "iterator" || hasTargets(symprop))
            continue;
        const symGlobal = symprop.operands[0]!;
        if (symGlobal.op !== "get_global" || symGlobal.imms.atom !== "Symbol") continue;
        if (arr.op !== "make_array" || arr.imms.len !== undefined) continue;
        if (!soleUse(it, w) || !soleUse(itfn, it) || !soleUse(symprop, itfn)) continue;
        if (!usesOf(useMap, arr).every((u) => (u.inst === itfn && u.index === 0) || (u.inst === it && u.index === 1)))
            continue;

        // wrapper uses: getNextValue getters + their calls, nothing else
        const getters = new Set<Inst>();
        const calls: Inst[] = [];
        let ok = true;
        for (const u of usesOf(useMap, w)) {
            const i = u.inst;
            if (
                i.op === "get_prop_atom" &&
                i.imms.atom === "getNextValue" &&
                u.index === 0 &&
                !hasTargets(i)
            ) {
                getters.add(i);
            } else if (
                i.op === "call" &&
                i.operands.length === 2 &&
                u.index === 1 &&
                !i.imms.direct &&
                !hasTargets(i) &&
                i.block === w.block
            ) {
                calls.push(i);
            } else {
                ok = false;
                break;
            }
        }
        if (!ok || calls.length !== getters.size) continue;
        for (const c of calls) if (!getters.has(c.operands[0]!) || !soleUse(c.operands[0]!, c)) ok = false;
        if (!ok) continue;

        // k-th call in block order sees element k
        calls.sort((a, b) => w.block!.insts.indexOf(a) - w.block!.insts.indexOf(b));
        for (let k = 0; k < calls.length; k++) {
            const el = k < arr.operands.length ? arr.operands[k]! : constUndefinedBefore(fn, calls[k]!);
            foldRead(useMap, fn, calls[k]!, el);
        }
        for (const g of getters) removeInst(useMap, g);
        removeInst(useMap, w);
        removeInst(useMap, it);
        removeInst(useMap, itfn);
        removeInst(useMap, symprop);
        if (usesOf(useMap, symGlobal).length === 0) removeInst(useMap, symGlobal);
        // the array itself is now unused (or write-only) — the sinking
        // pass and DCE finish it off
        stats.iters_folded++;
        changed = true;
    }
    return changed;
}

// --- unbox/box annihilation ---------------------------------------------------

// unbox_f64(box_f64(x)) is x: box_f64 always produces a genuinely boxed
// number, so the round-trip is the identity (modulo NaN canonicalization,
// which JS semantics cannot observe — a non-canonical NaN payload only
// ever flows into f64 ops, where any NaN behaves alike, or into a later
// box_f64, which canonicalizes).  Phase 3.6 clones lean on this: formals
// are boxed once at entry and trusted arithmetic re-unboxes them.
function foldUnboxOfBox(fn: Func, stats: OptStats): boolean {
    const boxFolds: Inst[] = [];
    const constFolds: Inst[] = [];
    fn.forEachInst((inst) => {
        if (inst.op !== "unbox_f64") return;
        const src = inst.operands[0]!;
        if (src.op === "box_f64") boxFolds.push(inst);
        else if (src.op === "const" && src.imms["kind"] === "number") constFolds.push(inst);
    });
    for (const u of boxFolds) {
        replaceAllUses(fn, u, u.operands[0]!.operands[0]!);
        const b = u.block!;
        const idx = b.insts.indexOf(u);
        if (idx >= 0) b.insts.splice(idx, 1);
        u.block = null;
        stats.unbox_folds++;
    }
    // unbox_f64(const number) is just the raw constant — rewrite the
    // unbox in place to f64_const (same Inst object keeps every use)
    for (const u of constFolds) {
        u.imms = { value: u.operands[0]!.imms["value"] };
        u.op = "f64_const";
        u.operands.length = 0;
        stats.unbox_folds++;
    }
    return boxFolds.length > 0 || constFolds.length > 0;
}

// --- dead instruction elimination --------------------------------------------

// dead-removable: unused results whose computation is unobservable.
// READ|GC effects are fine (a dead read never happens); THROW/WRITE/CALL
// are not — except the literal alloc ops, whose WRITE is to their own
// fresh storage.
function removableWhenDead(inst: Inst): boolean {
    if (inst.op === "blockparam") return false;
    if (inst.targets && inst.targets.length > 0) return false;
    if (inst.op === "make_object" || inst.op === "make_array" || inst.op === "make_object_shaped")
        return true;
    const info = opInfo(inst.op);
    if (info.terminator) return false;
    return (info.effects & ~(Effect.READ | Effect.GC)) === 0;
}

function eliminateDead(fn: Func, stats: OptStats): boolean {
    // use counts over operands and edge arguments, indexed by inst.id
    const counts = new Array<number>(fn.next_value_id).fill(0);
    const bump = (v: Inst) => {
        counts[v.id] = (counts[v.id] ?? 0) + 1;
    };
    fn.forEachInst((inst) => {
        for (const o of inst.operands) bump(o);
        if (inst.targets) {
            for (const t of inst.targets) for (const a of t.args) if (a) bump(a);
        }
    });

    const worklist: Inst[] = [];
    fn.forEachInst((inst) => {
        if (!counts[inst.id] && removableWhenDead(inst)) worklist.push(inst);
    });

    let changed = false;
    while (worklist.length > 0) {
        const inst = worklist.pop()!;
        if (!inst.block) continue;
        const b = inst.block;
        const idx = b.insts.indexOf(inst);
        if (idx >= 0) b.insts.splice(idx, 1);
        inst.block = null;
        stats.dead_removed++;
        // a shaped alloc reaching DCE means its reads/guards all folded
        // (or it was never consumed) — that IS the sink completing
        if (inst.op === "make_object_shaped") stats.shape_allocs_sunk++;
        changed = true;
        for (const o of inst.operands) {
            const n = --counts[o.id]!;
            if (n === 0 && o.block && removableWhenDead(o)) worklist.push(o);
        }
    }
    return changed;
}

// --- driver -------------------------------------------------------------------

export function optimizeFunction(fn: Func, module?: Module, stats?: OptStats): OptStats {
    const s = stats || newStats();
    // to fixpoint: inlining an IIFE exposes its env and literals;
    // sinking an outer literal can un-escape one nested inside it (its
    // only use was as the outer's operand)
    let rounds = 0;
    for (;;) {
        let changed = module ? inlineDirectCalls(module, fn, s) : false;
        if (eliminateDead(fn, s)) changed = true; // kill the closure before judging its env
        // one use scan per round, kept accurate by the mutation helpers
        const useMap = buildUseMap(fn);
        if (scalarReplaceEnvs(useMap, fn, s)) changed = true;
        if (foldIteratorWrappers(useMap, fn, s)) changed = true;
        if (sinkAllocations(useMap, fn, module, s)) changed = true;
        // shaped sinking folds guard branches; reclaim the dead arms so
        // the next round's use map lets the alloc itself drain
        if (sweepUnreachableBlocks(fn)) changed = true;
        if (eliminateDead(fn, s)) changed = true;
        if (!changed || ++rounds > 10) break;
    }
    // Phase 3.4: guard-region passes over the --types diamonds.  They run
    // after the general fixpoint (env scalarization has exposed the SSA
    // values the diamonds guard) and bail immediately when lowering
    // emitted no number guards — every flag-off compile.
    if (optimizeGuardRegions(fn, s)) eliminateDead(fn, s);
    // shapes-plan P4.3: shape-guard region merging + fact folding (bails
    // immediately without has_shape guards — every flag-off compile).
    // P4.5: a short fixpoint with rawJoinParams — heterogeneous merges
    // expose raw joins, and a raw join linearizes a fast side the next
    // shape-region match can grow through.
    for (let i = 0; i < 8; i++) {
        let ch = false;
        if (optimizeShapeRegions(fn, module, s)) {
            eliminateDead(fn, s);
            ch = true;
        }
        if (rawJoinParams(fn, s)) {
            eliminateDead(fn, s);
            ch = true;
        }
        if (!ch) break;
    }
    // Phase 3.6 cleanups.  These run AFTER the guard-region passes: the
    // merge machinery pattern-matches diamond fast arms (unbox of the
    // guarded value / of a literal const), so annihilating round-trips
    // or rewriting const unboxes earlier would refuse valid merges.
    if (foldUnboxOfBox(fn, s)) eliminateDead(fn, s);
    if (threadBooleanJoins(fn, s)) eliminateDead(fn, s);
    return s;
}

export function optimizeModule(m: Module): OptStats {
    const stats = newStats();
    for (const fn of m.functions) optimizeFunction(fn, m, stats);
    return stats;
}
