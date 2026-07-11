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

import { Func, Inst, Module, replaceAllUses } from "./ir";
import { Effect, opInfo } from "./ops";

export interface OptStats {
    allocs_sunk: number;
    reads_folded: number;
    calls_inlined: number;
    dead_removed: number;
}

function newStats(): OptStats {
    return { allocs_sunk: 0, reads_folded: 0, calls_inlined: 0, dead_removed: 0 };
}

// uses of `value` within fn, with enough position info to classify
interface Use {
    inst: Inst;
    // operand index, or -1 for a branch-edge argument
    index: number;
}

function usesOf(fn: Func, value: Inst): Use[] {
    const uses: Use[] = [];
    fn.forEachInst((inst) => {
        for (let i = 0; i < inst.operands.length; i++) {
            if (inst.operands[i] === value) uses.push({ inst, index: i });
        }
        if (inst.targets) {
            for (const t of inst.targets) {
                for (const a of t.args) {
                    if (a === value) uses.push({ inst, index: -1 });
                }
            }
        }
    });
    return uses;
}

function removeInst(inst: Inst): void {
    const b = inst.block!;
    const idx = b.insts.indexOf(inst);
    if (idx >= 0) b.insts.splice(idx, 1);
    inst.block = null;
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
function classifyUses(fn: Func, alloc: Inst): AllocUses {
    const r: AllocUses = { atomReads: [], atomWrites: [], computedReads: [], escapes: false };
    for (const use of usesOf(fn, alloc)) {
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
function foldRead(fn: Func, read: Inst, value: Inst): void {
    replaceAllUses(fn, read, value);
    removeInst(read);
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
function sinkAlloc(fn: Func, alloc: Inst, stats: OptStats): boolean {
    const isArray = alloc.op === "make_array";
    const uses = classifyUses(fn, alloc);
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
                foldRead(fn, read, constNumberBefore(fn, read, arrayLength(alloc)));
                stats.reads_folded++;
                changed = true;
            }
            for (const read of uses.computedReads) {
                if (read.targets) continue;
                const key = read.operands[1]!;
                if (key.op !== "const" || key.imms.kind !== "number") continue;
                const el = ownArrayElement(alloc, key.imms.value as number);
                if (!el) continue; // hole or out of range: prototype read
                foldRead(fn, read, el);
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
            foldRead(fn, read, v);
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
    const remaining = classifyUses(fn, alloc);
    if (
        !remaining.escapes &&
        remaining.atomReads.length === 0 &&
        remaining.computedReads.length === 0 &&
        remaining.atomWrites.every((w) => !w.targets && ownWrite(w))
    ) {
        for (const w of remaining.atomWrites) removeInst(w);
        removeInst(alloc);
        stats.allocs_sunk++;
        changed = true;
    }
    return changed;
}

function sinkAllocations(fn: Func, stats: OptStats): boolean {
    const candidates: Inst[] = [];
    fn.forEachInst((inst) => {
        if (inst.op === "make_object" || inst.op === "make_array") candidates.push(inst);
    });
    let changed = false;
    for (const c of candidates) {
        if (!c.block) continue; // removed by an earlier candidate's fold
        if (sinkAlloc(fn, c, stats)) changed = true;
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
    removeInst(call);
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
function scalarReplaceEnvs(fn: Func, stats: OptStats): boolean {
    let changed = false;
    const candidates: Inst[] = [];
    fn.forEachInst((inst) => {
        if (inst.op === "make_env") candidates.push(inst);
    });

    for (const env of candidates) {
        if (!env.block) continue;
        let ok = true;
        for (const use of usesOf(fn, env)) {
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
        for (const [load, v] of loads) foldRead(fn, load, v || constUndefinedBefore(fn, load));
        for (const s of stores) removeInst(s);
        removeInst(env);
        stats.allocs_sunk++;
        stats.reads_folded += loads.length;
        changed = true;
    }
    return changed;
}

// --- dead instruction elimination --------------------------------------------

// dead-removable: unused results whose computation is unobservable.
// READ|GC effects are fine (a dead read never happens); THROW/WRITE/CALL
// are not — except the literal alloc ops, whose WRITE is to their own
// fresh storage.
function removableWhenDead(inst: Inst): boolean {
    if (inst.op === "blockparam") return false;
    if (inst.targets && inst.targets.length > 0) return false;
    if (inst.op === "make_object" || inst.op === "make_array") return true;
    const info = opInfo(inst.op);
    if (info.terminator) return false;
    return (info.effects & ~(Effect.READ | Effect.GC)) === 0;
}

function eliminateDead(fn: Func, stats: OptStats): boolean {
    // use counts over operands and edge arguments
    const counts = new Map<Inst, number>();
    const bump = (v: Inst) => counts.set(v, (counts.get(v) || 0) + 1);
    fn.forEachInst((inst) => {
        for (const o of inst.operands) bump(o);
        if (inst.targets) {
            for (const t of inst.targets) for (const a of t.args) if (a) bump(a);
        }
    });

    const worklist: Inst[] = [];
    fn.forEachInst((inst) => {
        if (!counts.get(inst) && removableWhenDead(inst)) worklist.push(inst);
    });

    let changed = false;
    while (worklist.length > 0) {
        const inst = worklist.pop()!;
        if (!inst.block) continue;
        removeInst(inst);
        stats.dead_removed++;
        changed = true;
        for (const o of inst.operands) {
            const n = counts.get(o)! - 1;
            counts.set(o, n);
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
        if (scalarReplaceEnvs(fn, s)) changed = true;
        if (sinkAllocations(fn, s)) changed = true;
        if (eliminateDead(fn, s)) changed = true;
        if (!changed || ++rounds > 10) break;
    }
    return s;
}

export function optimizeModule(m: Module): OptStats {
    const stats = newStats();
    for (const fn of m.functions) optimizeFunction(fn, m, stats);
    return stats;
}
