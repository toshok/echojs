/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// Constructor-result sinking (docs/sinking-plan.md).
//
// A `new Point(x, y)` of a module-local, fence-passing constructor
// allocates an object whose fields are exactly the arguments — but the
// body's `this.x = x` stores are [[Set]] semantics, so deleting them
// statically is unsound: an accessor (or non-writable data property)
// later installed on the prototype chain must intercept every
// subsequent construction.  The runtime's accessor epoch
// (_ejs_accessor_epoch, ejs-object.h) turns that global hazard into one
// load: while the epoch is still zero, NO user code has installed
// anything interception-capable anywhere, so the construct is
// observably equivalent to a fresh shaped literal of its arguments.
//
// A qualifying construct site is rewritten into an epoch-guarded
// diamond:
//
//       %e = epoch_check
//       cond_br %e -> ^virtual, ^slow
//   ^virtual: a CLONE of the construct's use region, with the construct
//       replaced by `make_object_shaped(args)` — non-escaping by the
//       screens below, so the existing shaped-literal sinking drains
//       the allocation, guards, and reads to pure data flow;
//   ^slow: the ORIGINAL region, construct and real reads intact —
//       interception semantics preserved from the first bumped epoch on.
//
// The screens are all fail-closed, and jointly guarantee the virtual
// clone's allocation always drains (all-or-nothing: a virtual arm that
// kept the allocation would carry the wrong prototype):
//
//   - the callee resolves through a promoted (module-private) "%self"
//     slot with a single closure store, and EVERY load of that slot is
//     used only as a call/construct callee — which also proves the
//     ctor's `.prototype` is never read or replaced (a swapped
//     prototype could interpose an exotic object the epoch never sees);
//   - the ctor's lowered body is exactly the born-with-shape fill
//     diamond plus `return undefined`, its fill operands exactly the
//     formals in order; the construct passes exactly that many args
//     (a missing argument would change the runtime-derived shape);
//   - the result's uses classify like the shaped-literal sink's, and a
//     fold simulation (same guard-resolution rule as sinkShapedAlloc)
//     proves every use either folds or sits in an arm the folded guards
//     unreach;
//   - the use region is a single-entry single-exit acyclic subgraph of
//     plain br/cond_br blocks, so it can be duplicated wholesale.
//
// EJS_NO_CTOR_SINK=1 bisects this pass alone.

import { Block, Func, Inst, Module, ShapeField } from "./ir";
import { Effect, opInfo } from "./ops";
import { computeRPO, computeDominators, dominates } from "./verifier";

// region size cap: a use region bigger than this is not a constructor
// kernel, and cloning it would bloat code for a marginal win
const REGION_BLOCK_CAP = 24;

interface CtorMatch {
    fn: Func;
    shape: string;
    fields: readonly ShapeField[];
}

// does the lowered function body consist of exactly the fenced
// constructor prefix — has_shape(this,"") diamond around a
// fill_object_shaped of the formals — and `return undefined`?
function matchShapedCtor(m: Module, fn: Func): CtorMatch | null {
    if (fn.blocks.length !== 4 || fn.sig) return null;
    const entry = fn.entry!;
    if (entry.params.length < 3) return null; // [%env, %this, formals...]
    if (entry.insts.length !== 2) return null;
    const thisParam = entry.params[1]!;

    const guard = entry.insts[0]!;
    const cbr = entry.insts[1]!;
    if (guard.op !== "has_shape" || guard.imms["shape"] !== "") return null;
    if (guard.operands[0] !== thisParam) return null;
    if (cbr.op !== "cond_br" || cbr.operands[0] !== guard) return null;

    const fast = cbr.targets![0]!.block;
    const slow = cbr.targets![1]!.block;
    if (fast.params.length > 0 || slow.params.length > 0) return null;

    // fast arm: exactly the fill + br
    if (fast.insts.length !== 2) return null;
    const fill = fast.insts[0]!;
    const fastBr = fast.insts[1]!;
    if (fill.op !== "fill_object_shaped" || fill.targets) return null;
    if (fastBr.op !== "br" || fastBr.targets![0]!.args.length > 0) return null;
    const join = fastBr.targets![0]!.block;
    if (join.params.length > 0) return null;

    const shape = fill.imms["shape"] as string;
    const fields = m.shapes.get(shape);
    if (!fields) return null;
    const n = fields.length;
    if (entry.params.length !== 2 + n) return null;
    if (fill.operands.length !== 1 + n) return null;
    if (fill.operands[0] !== thisParam) return null;
    for (let i = 0; i < n; i++) if (fill.operands[i + 1] !== entry.params[i + 2]) return null;

    // slow arm: the sequential twin stores, one per field, then br join
    if (slow.insts.length !== n + 1) return null;
    for (let i = 0; i < n; i++) {
        const s = slow.insts[i]!;
        if (s.op !== "set_prop_atom" || s.targets) return null;
        if (s.operands[0] !== thisParam || s.operands[1] !== entry.params[i + 2]) return null;
        if (s.imms["atom"] !== fields[i]!.name) return null;
    }
    const slowBr = slow.insts[n]!;
    if (slowBr.op !== "br" || slowBr.targets![0]!.block !== join) return null;
    if (slowBr.targets![0]!.args.length > 0) return null;

    // join: return undefined, nothing else
    if (join.insts.length !== 2) return null;
    const undef = join.insts[0]!;
    const ret = join.insts[1]!;
    if (undef.op !== "const" || undef.imms["kind"] !== "undefined") return null;
    if (ret.op !== "return" || ret.operands[0] !== undef) return null;

    return { fn, shape, fields };
}

// module-wide uses of every value, as (user, operandIndex) with -1 for
// branch-edge arguments — the specialization pass's shape
interface Use {
    fn: Func;
    user: Inst;
    operandIndex: number;
}

function usesInModule(m: Module): Map<Inst, Use[]> {
    const uses = new Map<Inst, Use[]>();
    const add = (v: Inst, u: Use) => {
        let list = uses.get(v);
        if (!list) uses.set(v, (list = []));
        list.push(u);
    };
    for (const fn of m.functions) {
        fn.forEachInst((inst) => {
            inst.operands.forEach((o, i) => add(o, { fn, user: inst, operandIndex: i }));
            if (inst.targets)
                for (const t of inst.targets)
                    for (const a of t.args) if (a) add(a, { fn, user: inst, operandIndex: -1 });
        });
    }
    return uses;
}

// the shaped-literal sink's repr-provability rule: folding a shape
// guard TRUE exposes raw f64 slot loads, so every f64 field's operand
// must provably be a number
function provablyNumber(v: Inst): boolean {
    return v.op === "box_f64" || (v.op === "const" && v.imms["kind"] === "number");
}

// a resolved constructor slot: the closure store and the matched ctor
interface SlotCtor {
    match: CtorMatch;
    store: Inst;
    storeFn: Func;
    prefixSafe: boolean;
}

// is `a` before `b` under the dominator tree of their shared function?
function comesBefore(idom: Map<Block, Block>, a: Inst, b: Inst): boolean {
    const ba = a.block!;
    const bb = b.block!;
    if (ba === bb) return ba.insts.indexOf(a) < bb.insts.indexOf(b);
    return dominates(idom, ba, bb);
}

interface Candidate {
    fn: Func;
    construct: Inst;
    match: CtorMatch;
}

interface Region {
    blocks: Set<Block>;
    exit: Block;
}

// the single-entry single-exit acyclic region rooted at `entry` that
// contains every block of `useBlocks`.  null when no such region exists
// (multiple exits, outside predecessors, cycles, non-branch
// terminators, catch blocks, or over the cap).
function computeRegion(entry: Block, useBlocks: Set<Block>): Region | null {
    // predecessor closure from the uses up to the entry
    const blocks = new Set<Block>([entry, ...useBlocks]);
    const wl: Block[] = [...useBlocks];
    while (wl.length > 0) {
        const b = wl.pop()!;
        if (b === entry) continue;
        for (const p of b.preds()) {
            if (!blocks.has(p)) {
                blocks.add(p);
                wl.push(p);
                if (blocks.size > REGION_BLOCK_CAP) return null;
            }
        }
    }
    // structural screens + the unique exit
    let exit: Block | null = null;
    for (const b of blocks) {
        if (b.isCatch) return null;
        const t = b.terminator;
        if (!t || (t.op !== "br" && t.op !== "cond_br")) return null;
        for (const inst of b.insts) if (inst !== t && inst.targets) return null;
        if (b !== entry) {
            for (const p of b.preds()) if (!blocks.has(p)) return null;
            if (b.params.some((p) => p.isException)) return null;
        }
        for (const s of b.succs()) {
            if (blocks.has(s)) continue;
            if (exit && exit !== s) return null;
            exit = s;
        }
    }
    if (!exit) return null;
    // acyclic + entry-reaches-all, by DFS with an on-stack set
    const state = new Map<Block, 1 | 2>(); // 1 = on stack, 2 = done
    const visit = (b: Block): boolean => {
        state.set(b, 1);
        for (const s of b.succs()) {
            if (!blocks.has(s)) continue;
            const st = state.get(s);
            if (st === 1) return false; // back edge: cycle
            if (st === undefined && !visit(s)) return false;
        }
        state.set(b, 2);
        return true;
    };
    if (!visit(entry)) return null;
    for (const b of blocks) if (state.get(b) !== 2) return null; // unreachable from entry
    return { blocks, exit };
}

// simulate the shaped-literal sink's guard folding over the region:
// from `entry`, a cond_br whose condition is a sole-use has_shape guard
// on `result` takes only its statically resolved edge; everything else
// takes all in-region edges.  Returns the reachable block set.
function foldReachable(
    region: Region,
    entry: Block,
    result: Inst,
    shape: string,
    reprsProven: boolean,
    guardOf: Map<Inst, Inst> // cond_br -> its foldable has_shape guard
): Set<Block> {
    const reach = new Set<Block>([entry]);
    const wl: Block[] = [entry];
    while (wl.length > 0) {
        const b = wl.pop()!;
        const t = b.terminator!;
        let succs: Block[];
        const guard = t.op === "cond_br" ? guardOf.get(t) : undefined;
        if (guard && guard.operands[0] === result) {
            const takeTrue = guard.imms["shape"] === shape && reprsProven;
            succs = [t.targets![takeTrue ? 0 : 1]!.block];
        } else {
            succs = b.succs();
        }
        for (const s of succs) {
            if (!region.blocks.has(s) || reach.has(s)) continue;
            reach.add(s);
            wl.push(s);
        }
    }
    return reach;
}

// rewrite every qualifying construct site in the module.  Returns the
// number of sites rewritten; the caller re-runs the optimizer so the
// shaped-literal sink can drain the planted virtual allocations.
export function sinkConstructResults(
    m: Module,
    promotedSlots: Set<number>,
    toplevelName: string | null
): number {
    if (process.env["EJS_NO_CTOR_SINK"]) return 0;
    if (m.shapes.size === 0) return 0;

    const toplevelFn = toplevelName
        ? m.functions.find((f) => f.name === toplevelName) || null
        : null;

    // %self slot traffic, module-wide
    const selfStores = new Map<number, Inst[]>();
    const selfLoads = new Map<number, Inst[]>();
    const storeFns = new Map<Inst, Func>();
    for (const fn of m.functions) {
        fn.forEachInst((inst) => {
            if (inst.imms["module"] !== "%self") return;
            const slot = inst.imms["slot"] as number;
            if (inst.op === "module_slot_store") {
                let l = selfStores.get(slot);
                if (!l) selfStores.set(slot, (l = []));
                l.push(inst);
                storeFns.set(inst, fn);
            } else if (inst.op === "module_slot_load") {
                let l = selfLoads.get(slot);
                if (!l) selfLoads.set(slot, (l = []));
                l.push(inst);
            }
        });
    }
    if (selfStores.size === 0) return 0;

    const uses = usesInModule(m);
    const calleeUse = (u: Use): boolean =>
        u.operandIndex === 0 &&
        ((u.user.op === "call" && !u.user.imms["direct"]) || u.user.op === "construct");

    // resolve each promoted slot that provably always holds one
    // fence-passing constructor closure whose loads are all callees
    const slotCtors = new Map<number, SlotCtor>();
    for (const [slot, stores] of selfStores) {
        if (!promotedSlots.has(slot)) continue;
        if (stores.length !== 1) continue;
        const store = stores[0]!;
        const closure = store.operands[0]!;
        if (closure.op !== "make_closure") continue;
        const ctorFn = m.functions.find((f) => f.name === closure.imms["fn"]);
        if (!ctorFn) continue;
        const match = matchShapedCtor(m, ctorFn);
        if (!match) continue;
        // every load only a callee; every OTHER use of the closure value
        // is just the store itself (the specialization discipline —
        // anything else could reach `.prototype`)
        let ok = true;
        for (const u of uses.get(closure) || []) {
            if (u.user === store && u.operandIndex === 0) continue;
            if (u.operandIndex === -1 || !calleeUse(u)) {
                ok = false;
                break;
            }
        }
        if (ok)
            for (const load of selfLoads.get(slot) || []) {
                for (const lu of uses.get(load) || []) {
                    if (lu.operandIndex === -1 || !calleeUse(lu)) {
                        ok = false;
                        break;
                    }
                }
                if (!ok) break;
            }
        if (!ok) continue;

        const storeFn = storeFns.get(store)!;
        // cross-function resolution needs the store to precede all user
        // code: toplevel entry block, nothing CALL-shaped before it.
        // (The specialization pass's rule; the loads themselves are
        // pure so only calls could observe the slot in between.)
        let prefixSafe = false;
        if (toplevelFn && storeFn === toplevelFn && store.block === toplevelFn.entry) {
            prefixSafe = true;
            for (const inst of toplevelFn.entry!.insts) {
                if (inst === store) break;
                if ((opInfo(inst.op).effects & Effect.CALL) !== 0) {
                    prefixSafe = false;
                    break;
                }
            }
        }
        slotCtors.set(slot, { match, store, storeFn, prefixSafe });
    }
    if (slotCtors.size === 0) return 0;

    // enumerate qualifying construct sites
    const candidates: Candidate[] = [];
    const idoms = new Map<Func, Map<Block, Block>>();
    const idomOf = (fn: Func): Map<Block, Block> => {
        let d = idoms.get(fn);
        if (!d) {
            const { rpo } = computeRPO(fn);
            idoms.set(fn, (d = computeDominators(fn, rpo)));
        }
        return d;
    };
    for (const fn of m.functions) {
        fn.forEachInst((inst) => {
            if (inst.op !== "construct" || (inst.targets && inst.targets.length > 0)) return;
            const callee = inst.operands[0]!;
            if (callee.op !== "module_slot_load" || callee.imms["module"] !== "%self") return;
            const sc = slotCtors.get(callee.imms["slot"] as number);
            if (!sc) return;
            // the load must provably observe the (single) store
            const orderOk = sc.prefixSafe
                ? !(
                      callee.block === sc.store.block &&
                      sc.store.block!.insts.indexOf(callee) <
                          sc.store.block!.insts.indexOf(sc.store)
                  )
                : fn === sc.storeFn && comesBefore(idomOf(fn), sc.store, callee);
            if (!orderOk) return;
            if (inst.operands.length - 1 !== sc.match.fields.length) return;
            candidates.push({ fn, construct: inst, match: sc.match });
        });
    }

    // a successful sink splits/clones blocks, so the module-wide use map
    // goes stale — rebuild it before judging the next site
    let sunk = 0;
    let freshUses = uses;
    for (const c of candidates) {
        if (sinkOneSite(c, freshUses)) {
            sunk++;
            freshUses = usesInModule(m);
        }
    }
    return sunk;
}

function sinkOneSite(c: Candidate, uses: Map<Inst, Use[]>): boolean {
    const { fn, construct, match } = c;
    const result = construct;
    const shape = match.shape;
    const fields = match.fields;
    const args = construct.operands.slice(1);

    // classify the result's uses: shape guards whose sole consumer is
    // their block's cond_br, slot loads, own-field atom reads.  Anything
    // else declines the site.
    const resultUses = uses.get(result) || [];
    const guardOf = new Map<Inst, Inst>(); // cond_br -> guard
    const useBlocks = new Set<Block>();
    const guards: Inst[] = [];
    const slotReads: Inst[] = [];
    const atomReads: Inst[] = [];
    for (const u of resultUses) {
        const { user, operandIndex } = u;
        if (operandIndex === -1 || user.block === null) return false;
        if (user.op === "has_shape" && operandIndex === 0) {
            const gu = uses.get(user) || [];
            const cbr = user.block.terminator;
            if (
                gu.length !== 1 ||
                gu[0]!.user !== cbr ||
                gu[0]!.operandIndex !== 0 ||
                !cbr ||
                cbr.op !== "cond_br"
            )
                return false;
            guardOf.set(cbr, user);
            guards.push(user);
        } else if (user.op === "slot_load" && operandIndex === 0) {
            slotReads.push(user);
        } else if (user.op === "get_prop_atom" && operandIndex === 0) {
            atomReads.push(user);
        } else {
            return false;
        }
        useBlocks.add(user.block);
    }
    if (useBlocks.size === 0) return false; // nothing to virtualize

    const region = computeRegion(construct.block!, useBlocks);
    if (!region) return false;

    // fold simulation: every use must fold (guards resolve, reads fold
    // to operands) or sit in an arm the folded guards make unreachable —
    // the guarantee that the virtual clone's allocation fully drains
    const reprsProven = fields.every(
        (f, i) => f.repr !== "f64" || provablyNumber(args[i]!)
    );
    const reach = foldReachable(region, construct.block!, result, shape, reprsProven, guardOf);
    const fieldIndex = (name: string): number => {
        for (let i = 0; i < fields.length; i++) if (fields[i]!.name === name) return i;
        return -1;
    };
    // (guards need no reachability screen: only sole-use cond_br guards
    // got this far, and those always resolve statically)
    for (const r of slotReads) {
        if (!reach.has(r.block!)) continue;
        if (r.targets) return false;
        if (r.imms["shape"] !== shape) return false;
        const k = r.imms["slot"] as number;
        if (k < 0 || k >= fields.length) return false;
        if (r.imms["repr"] === "f64" && !provablyNumber(args[k]!)) return false;
    }
    for (const r of atomReads) {
        if (!reach.has(r.block!)) continue;
        if (r.targets) return false;
        if (fieldIndex(r.imms["atom"] as string) < 0) return false; // prototype read
    }

    // live-outs: values defined in the region (below the construct) and
    // used at-or-after the exit need a join param each
    const regionDefs = new Set<Inst>();
    const b0 = construct.block!;
    const splitAt = b0.insts.indexOf(construct);
    for (const b of region.blocks) {
        for (const p of b.params) if (b !== b0) regionDefs.add(p);
        const from = b === b0 ? splitAt : 0;
        for (let i = from; i < b.insts.length; i++) regionDefs.add(b.insts[i]!);
    }
    const liveOuts: Inst[] = [];
    for (const v of regionDefs) {
        for (const u of uses.get(v) || []) {
            if (u.user.block && !region.blocks.has(u.user.block)) {
                // an i1 can never cross a block boundary, raw or joined
                if (v.type === "i1") return false;
                liveOuts.push(v);
                break;
            }
        }
    }
    if (liveOuts.length > 0) {
        // the exit's params can only absorb them if every exit
        // predecessor is ours
        for (const p of region.exit.preds()) if (!region.blocks.has(p)) return false;
    }

    // ---- rewrite -------------------------------------------------
    // 1. split the construct's block: the head keeps everything before
    //    the construct and gains the epoch diamond; the tail (construct
    //    included) becomes the slow arm's entry.  Successor predEdges
    //    reference terminator INSTRUCTIONS, so moving the instructions
    //    keeps the edge bookkeeping consistent.
    const slowEntry = new Block(fn, "ctor_slow");
    slowEntry.sealed = true;
    fn.blocks.push(slowEntry);
    slowEntry.insts = b0.insts.splice(splitAt);
    for (const inst of slowEntry.insts) inst.block = slowEntry;

    const cloneOf = new Map<Block, Block>();
    const regionBlocks: Block[] = [slowEntry];
    for (const b of region.blocks) if (b !== b0) regionBlocks.push(b);

    // 2. clone the region; the construct becomes a shaped literal of
    //    the arguments
    const valueMap = new Map<Inst, Inst>();
    for (const b of regionBlocks) {
        const cb = new Block(fn, "ctor_virtual");
        cb.sealed = true;
        fn.blocks.push(cb);
        cloneOf.set(b, cb);
        for (const p of b.params) {
            const cp = cb.addParam(p.nameHint);
            cp.type = p.type;
            cp.rawJoin = p.rawJoin;
            valueMap.set(p, cp);
        }
    }
    const mapVal = (v: Inst): Inst => valueMap.get(v) || v;
    // live-out join params, appended to the exit's existing ones.  The
    // slow arm's exit edges pass the original values, the clone's edges
    // the cloned ones; adding them BEFORE the clone's terminators exist
    // extends only the original edges with the null slots filled here.
    const exitParams = new Map<Inst, Inst>();
    for (const v of liveOuts) {
        const p = region.exit.addParam("ctor_sink");
        p.type = v.type;
        p.rawJoin = v.type === "f64";
        exitParams.set(v, p);
        for (const e of region.exit.predEdges) {
            const t = e.inst.targets![e.targetIndex]!;
            if (t.args[t.args.length - 1] === null) t.args[t.args.length - 1] = v;
        }
    }
    for (const b of regionBlocks) {
        const cb = cloneOf.get(b)!;
        for (const inst of b.insts) {
            let clone: Inst;
            if (inst === construct) {
                clone = new Inst(fn, "make_object_shaped", args.map(mapVal), { shape: shape });
            } else {
                clone = new Inst(fn, inst.op, inst.operands.map(mapVal), { ...inst.imms });
                clone.type = inst.type;
            }
            clone.block = cb;
            cb.insts.push(clone);
            valueMap.set(inst, clone);
            if (inst.targets) {
                for (const t of inst.targets) {
                    // exit edges were already extended with the live-out
                    // args above, so mapping the originals covers them
                    const target = cloneOf.get(t.block) || t.block;
                    clone.addTarget(target, t.args.map((a) => (a ? mapVal(a) : a)), t.kind);
                }
            }
        }
    }

    // 3. the epoch diamond closes the head
    const epoch = new Inst(fn, "epoch_check", [], {});
    epoch.block = b0;
    b0.insts.push(epoch);
    const cbr = new Inst(fn, "cond_br", [epoch], {});
    cbr.block = b0;
    b0.insts.push(cbr);
    cbr.addTarget(cloneOf.get(slowEntry)!, []);
    cbr.addTarget(slowEntry, []);

    // 4. everything at or beyond the exit sees the live-outs through
    //    the new join params
    if (liveOuts.length > 0) {
        const cloneSet = new Set(cloneOf.values());
        fn.forEachInst((inst) => {
            const b = inst.block;
            if (!b || region.blocks.has(b) || b === slowEntry || cloneSet.has(b)) return;
            for (let i = 0; i < inst.operands.length; i++) {
                const p = exitParams.get(inst.operands[i]!);
                if (p) inst.operands[i] = p;
            }
            if (inst.targets)
                for (const t of inst.targets)
                    for (let i = 0; i < t.args.length; i++) {
                        const a = t.args[i];
                        if (a) {
                            const p = exitParams.get(a);
                            if (p) t.args[i] = p;
                        }
                    }
        });
    }
    return true;
}
