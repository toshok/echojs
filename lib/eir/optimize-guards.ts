/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// Phase 3.4: trust-free optimizer passes over the Phase 3 guarded
// arithmetic diamonds (lower.ts numericDiamond).
//
//   (a) dominated-guard elimination + guard-region merging: a has_tag
//       "number" test on a value already proven number is folded, and
//       adjacent diamonds fuse into one guard region with one fast side
//       and ONE slow path;
//   (b) raw f64 block params for the joins the merge rewires, so the
//       merged fast side computes unboxed end-to-end and boxes exactly
//       once at the region exit.
//
// Both passes are trust-free: nothing here consumes an oracle claim.
// Every fact is proven from the IR itself, so a wrong oracle upstream
// still only costs speed, never correctness.
//
// ---- Soundness inventory (each rewrite's argument, in one place) ----
//
// Proven-number facts (provenNumberAt):
//   - const kind="number" and box_f64 results are numbers by
//     construction.
//   - results of the generic ops mul / div / sub are ALWAYS numbers:
//     ES semantics (`* / -` apply ToNumber and produce a Number; they
//     throw rather than return anything else) and the echojs runtime
//     agrees (runtime/ejs-ops.c _ejs_op_{mult,div,sub} only ever return
//     NUMBER_TO_EJSVAL(..)).  `add` is excluded (string concatenation)
//     unless both its operands are proven numbers.
//   - a block param is a number if every incoming edge argument is
//     proven (each argument's proof holds at the query block too —
//     number-ness of an immutable SSA value is position-independent
//     once every path establishes it; self-edges are vacuous).
//   - dominance facts: if block T is the sole-predecessor TRUE successor
//     of `cond_br (has_tag %v "number")` and T dominates B, then every
//     path to B passed the guard while it was true; SSA values are
//     immutable, so %v is a number at B.  This is the "real dominance
//     reasoning": the CHK dominator tree (verifier.ts) plus the
//     sole-pred-true-edge condition, which is exactly what makes
//     entering T equivalent to the guard having held.
//
// Guard folding: a cond_br on a proven has_tag rewrites to br to the
// true target.  Removing CFG edges only grows dominance, so folding
// with a momentarily-stale dominator tree is conservative.
//
// Region merging (the diamond CFG's structural argument): a region is
// verified — never assumed — to have the shape
//     head:  ... cond_br (has_tag) -> fast..., slow
//     fast side: blocks whose instructions are all effect-free (at most
//         GC), terminated by br / interior number guards (false edges
//         all to the region's slow entry) / i1 cond_brs, exiting to the
//         join;
//     slow side: a linear chain of blocks holding only the whitelisted
//         generic ops {add,sub,mul,div,lt} (plus effect-free
//         instructions and br), exiting to the same join.
// Merging region R1 with the region R2 headed at R1's join J1:
//   - R1's slow exit is retargeted from J1 straight into R2's slow
//     entry, and J1's params are substituted with the values that edge
//     carried wherever R2's slow chain used them: the slow path becomes
//     the full generic computation in original program order (identical
//     semantics — the generic ops ARE the JS semantics regardless of
//     operand types).  A previously slow-then-fast mixed execution now
//     runs fully generic: same observable behavior, only slower — the
//     documented cost model of guard regions.
//   - R2's guard-failure edges are retargeted from R2's slow entry to
//     R1's slow entry (the merged region's single slow path).  Those
//     failures happen only after R1's guards all passed and R1's fast
//     side (effect-free by the region check) ran, so the R1 portion of
//     the slow chain RE-executes.  That is sound because the merge
//     first proves every instruction in R1's slow chain is either
//     effect-free or a whitelisted generic op whose operands are proven
//     numbers at R1's fast exits: a generic op on numbers is pure, non-
//     throwing (its unwind edge stays untaken), and returns bit-for-bit
//     the f64 result the fast side already computed.
//   - values defined at J1 (params + the pure instruction prefix ahead
//     of R2's guard) that are still used beyond R2 are routed through
//     R2's join as new params — fast edges pass the J1 value, the slow
//     edge passes its slow-side substitute — after checking that every
//     such use IS dominated by that join (else the merge is refused).
//
// Raw f64 joins (pass b): a param is converted only when every incoming
// argument is a box_f64 result (whose only consumers are edges feeding
// converted params), an f64 value, or another converted param.  The
// boxes are stripped on the edges, unbox_f64 uses collapse to the param
// itself, and any remaining boxed use re-boxes ONCE at the head of the
// param's block — that is the single box at the region exit.  The
// verifier re-checks all of it (see verifier.ts rawJoin rules).

import { Func, Block, Inst } from "./ir";
import type { Target } from "./ir";
import { Effect, opInfo } from "./ops";
import { computeRPO, computeDominators, dominates } from "./verifier";
import type { OptStats } from "./optimize";

// generic ops that (1) lowering pairs with f64 fast ops, and (2) are
// pure and value-identical to the f64 op when both operands are numbers
// (see the soundness inventory above)
const SLOW_OPS = new Set(["add", "sub", "mul", "div", "lt"]);

// generic ops whose RESULT is always a number (ES + runtime/ejs-ops.c)
const NUMBER_RESULT_OPS = new Set(["mul", "div", "sub"]);

function isNumberGuard(inst: Inst): boolean {
    return inst.op === "has_tag" && inst.imms["tag"] === "number";
}

// --- CFG edge surgery -------------------------------------------------------

function removePredEdge(block: Block, inst: Inst, targetIndex: number): void {
    block.predEdges = block.predEdges.filter(
        (e) => !(e.inst === inst && e.targetIndex === targetIndex)
    );
}

// point inst.targets[targetIndex] at a new block, maintaining predEdges
function retargetEdge(inst: Inst, targetIndex: number, newBlock: Block, newArgs: Inst[]): void {
    const t = inst.targets![targetIndex]!;
    removePredEdge(t.block, inst, targetIndex);
    t.block = newBlock;
    t.args = newArgs;
    newBlock.predEdges.push({ inst: inst, targetIndex: targetIndex });
}

// replace a block's cond_br terminator with an unconditional br to
// targets[keepIndex] (edge args preserved); the condition goes dead and
// DCE sweeps it later
function condBrToBr(fn: Func, block: Block, keepIndex: number): void {
    const cbr = block.terminator!;
    const keep = cbr.targets![keepIndex]!;
    removePredEdge(keep.block, cbr, keepIndex);
    removePredEdge(cbr.targets![1 - keepIndex]!.block, cbr, 1 - keepIndex);
    block.insts.pop();
    cbr.block = null;
    const br = new Inst(fn, "br", [], {});
    br.block = block;
    block.insts.push(br);
    br.addTarget(keep.block, keep.args.slice());
}

// drop blocks no longer reachable from entry and rebuild predEdges so
// no stale edges (from deleted blocks) survive
function sweepUnreachableBlocks(fn: Func): boolean {
    const reachable = new Set<Block>([fn.entry!]);
    const stack: Block[] = [fn.entry!];
    while (stack.length > 0) {
        const b = stack.pop()!;
        for (const s of b.succs()) {
            if (!reachable.has(s)) {
                reachable.add(s);
                stack.push(s);
            }
        }
    }
    if (reachable.size === fn.blocks.length) return false;
    fn.blocks = fn.blocks.filter((b) => reachable.has(b));
    for (const b of fn.blocks) b.predEdges = [];
    for (const b of fn.blocks) {
        const t = b.terminator;
        if (!t || !t.targets) continue;
        t.targets.forEach((tg, i) => tg.block.predEdges.push({ inst: t, targetIndex: i }));
    }
    return true;
}

// --- proven-number reasoning ------------------------------------------------

// the value proven number on entry to `b` by b being the sole-pred TRUE
// successor of a number guard (see the soundness inventory)
function blockEntryFact(b: Block): Inst | null {
    if (b.predEdges.length !== 1) return null;
    const e = b.predEdges[0]!;
    if (e.targetIndex !== 0) return null;
    if (e.inst.op !== "cond_br") return null;
    const cond = e.inst.operands[0]!;
    if (!isNumberGuard(cond)) return null;
    return cond.operands[0]!;
}

// a dominance fact: some number guard on v has a sole-pred TRUE
// successor dominating `block`, so every path to `block` proved v
function guardFactAt(v: Inst, block: Block, idom: Map<Block, Block>): boolean {
    let b: Block = block;
    for (;;) {
        if (blockEntryFact(b) === v) return true;
        const n = idom.get(b);
        if (!n || n === b) return false;
        b = n;
    }
}

// is v proven number at `block`?  Combines value-intrinsic proofs
// (const/box_f64/mul/div/sub, position-independent) with dominance
// facts.  Facts are sound inside the recursion too: an SSA value's
// number-ness is immutable, so "every path to `block` passed a guard on
// x" proves x is a number at `block` no matter where x sits in a
// compound proof (an add's operand, a param's incoming argument).
// depth-capped so param cycles terminate.
function provenNumberAt(
    v: Inst,
    block: Block,
    idom: Map<Block, Block>,
    depth: number = 6
): boolean {
    if (v.op === "const") return v.imms["kind"] === "number";
    if (v.op === "box_f64") return true;
    if (NUMBER_RESULT_OPS.has(v.op)) return true;
    if (guardFactAt(v, block, idom)) return true;
    if (depth <= 0) return false;
    if (v.op === "add")
        return (
            provenNumberAt(v.operands[0]!, block, idom, depth - 1) &&
            provenNumberAt(v.operands[1]!, block, idom, depth - 1)
        );
    if (v.op === "blockparam" && !v.isException && v.block && !v.block.isCatch) {
        const b = v.block;
        if (b.predEdges.length === 0) return false;
        const argIdx = b.argIndexOfParam(v);
        let anyProven = false;
        for (const e of b.predEdges) {
            const arg = e.inst.targets![e.targetIndex]!.args[argIdx];
            if (!arg) return false;
            if (arg === v) continue; // self-edge: vacuous
            if (!provenNumberAt(arg, block, idom, depth - 1)) return false;
            anyProven = true;
        }
        return anyProven;
    }
    return false;
}

// --- pass (a) part 1: dominated/proven guard folding ------------------------

function foldProvenGuards(fn: Func, stats: OptStats): boolean {
    let changed = false;
    const { rpo } = computeRPO(fn);
    const idom = computeDominators(fn, rpo);
    // folding only REMOVES edges, so dominance only grows and a
    // momentarily-stale idom stays conservative; predEdges (which
    // blockEntryFact reads) are maintained live by condBrToBr.
    for (const b of fn.blocks) {
        const term = b.terminator;
        if (!term || term.op !== "cond_br") continue;
        const cond = term.operands[0]!;
        if (!isNumberGuard(cond)) continue;
        if (provenNumberAt(cond.operands[0]!, b, idom)) {
            condBrToBr(fn, b, 0);
            stats.guards_folded++;
            changed = true;
        }
    }
    if (changed) sweepUnreachableBlocks(fn);
    return changed;
}

// --- pass (a) part 2: guard-region recognition + merging --------------------

interface EdgeRef {
    inst: Inst;
    targetIndex: number;
}

interface GuardRegion {
    head: Block; // ends in cond_br on a number guard
    fastBlocks: Set<Block>; // true-side blocks strictly between head and join
    guardFalseEdges: EdgeRef[]; // every guard's false edge (all -> slowEntry)
    fastExitEdges: EdgeRef[]; // fast-side edges into the join
    slowEntry: Block;
    slowChain: Block[]; // slowEntry .. slow exit, linear
    slowSet: Set<Block>;
    slowExitEdge: EdgeRef; // the slow chain's edge into the join
    join: Block;
}

const MAX_REGION_BLOCKS = 40;

// structurally verify (not assume) the guard-region shape headed at
// `head`.  Returns null the moment anything deviates.
function matchRegionAt(head: Block): GuardRegion | null {
    const term = head.terminator;
    if (!term || term.op !== "cond_br") return null;
    const cond = term.operands[0]!;
    if (!isNumberGuard(cond)) return null;
    const t0 = term.targets![0]!;
    const t1 = term.targets![1]!;
    if (t0.args.length !== 0 || t1.args.length !== 0) return null;
    const slowEntry = t1.block;
    if (slowEntry.isCatch || t0.block.isCatch) return null;
    if (slowEntry.params.length !== 0) return null;
    if (t0.block === slowEntry) return null;

    // --- slow side: a linear chain of whitelisted generic ops
    const slowChain: Block[] = [];
    const slowSet = new Set<Block>();
    let join: Block | null = null;
    let slowExitEdge: EdgeRef | null = null;
    let sb = slowEntry;
    for (;;) {
        if (slowChain.length > MAX_REGION_BLOCKS) return null;
        if (slowSet.has(sb) || sb === head) return null;
        slowChain.push(sb);
        slowSet.add(sb);
        const bt = sb.terminator;
        if (!bt) return null;
        for (const inst of sb.insts) {
            if (inst === bt) continue;
            if (inst.targets && inst.targets.length > 0) return null;
            if (!SLOW_OPS.has(inst.op) && opInfo(inst.op).effects !== Effect.NONE) return null;
        }
        let exit: EdgeRef;
        if (bt.op === "br") {
            exit = { inst: bt, targetIndex: 0 };
        } else if (
            SLOW_OPS.has(bt.op) &&
            bt.targets &&
            bt.targets.length === 2 &&
            bt.targets[0]!.kind === "normal"
        ) {
            // a generic op inside a protected region: [normal, unwind]
            exit = { inst: bt, targetIndex: 0 };
        } else {
            return null;
        }
        const next = exit.inst.targets![exit.targetIndex]!.block;
        if (next.isCatch) return null;
        // interior slow blocks are reachable only from the chain; the
        // join is the first successor with an outside predecessor
        if (next.predEdges.every((e) => slowSet.has(e.inst.block!))) {
            sb = next;
            continue;
        }
        join = next;
        slowExitEdge = exit;
        break;
    }
    if (!join || join.isCatch || join === head) return null;

    // --- fast side: effect-free blocks from the true target to the join
    const fastBlocks = new Set<Block>();
    const guardFalseEdges: EdgeRef[] = [{ inst: term, targetIndex: 1 }];
    const fastExitEdges: EdgeRef[] = [];
    const work: Block[] = [t0.block];
    while (work.length > 0) {
        const fb = work.pop()!;
        if (fastBlocks.has(fb)) continue;
        if (fastBlocks.size > MAX_REGION_BLOCKS) return null;
        if (fb === join || fb === head || slowSet.has(fb) || fb.isCatch) return null;
        fastBlocks.add(fb);
        const ft: Inst | null = fb.terminator;
        if (!ft) return null;
        for (const inst of fb.insts) {
            if (inst === ft) continue;
            if (inst.targets && inst.targets.length > 0) return null;
            // at most GC (const/unbox/box/f64_*/has_tag): re-orderable
            // around nothing, skippable by nothing — the region never
            // skips or repeats fast blocks, this just proves they are
            // unobservable when the slow path re-runs their work
            if ((opInfo(inst.op).effects & ~Effect.GC) !== 0) return null;
        }
        if (ft.op === "br") {
            const tg: Target = ft.targets![0]!;
            // fast-internal br edges may carry args (a previous merge
            // leaves former joins — blocks with params — on the fast
            // side); the pred check below confirms membership
            if (tg.block === join) fastExitEdges.push({ inst: ft, targetIndex: 0 });
            else work.push(tg.block);
        } else if (ft.op === "cond_br") {
            const c = ft.operands[0]!;
            let arms: number[];
            if (isNumberGuard(c)) {
                const f = ft.targets![1]!;
                if (f.block !== slowEntry || f.args.length !== 0) return null;
                guardFalseEdges.push({ inst: ft, targetIndex: 1 });
                arms = [0];
            } else if (c.type === "i1") {
                arms = [0, 1]; // f64_lt-style split: both arms stay fast
            } else {
                return null;
            }
            for (const i of arms) {
                const tg: Target = ft.targets![i]!;
                if (tg.block === join) {
                    fastExitEdges.push({ inst: ft, targetIndex: i });
                } else {
                    if (tg.args.length !== 0) return null;
                    work.push(tg.block);
                }
            }
        } else {
            return null; // return/throw/invoke inside the fast side
        }
    }
    if (fastExitEdges.length === 0) return null;
    // the fast side is entered only through the head's guard
    for (const fb of fastBlocks) {
        for (const e of fb.predEdges) {
            const src = e.inst.block!;
            if (src !== head && !fastBlocks.has(src)) return null;
        }
    }

    return {
        head: head,
        fastBlocks: fastBlocks,
        guardFalseEdges: guardFalseEdges,
        fastExitEdges: fastExitEdges,
        slowEntry: slowEntry,
        slowChain: slowChain,
        slowSet: slowSet,
        slowExitEdge: slowExitEdge!,
        join: join,
    };
}

// merge the region headed at r1.join (if any) into r1.  Returns true if
// the CFG changed.  All checks precede all mutations.
function tryMergeAt(fn: Func, r1: GuardRegion, idom: Map<Block, Block>, stats: OptStats): boolean {
    const j1 = r1.join;
    const r2 = matchRegionAt(j1);
    if (!r2) return false;
    const j2 = r2.join;

    // region2 must live strictly below region1 (no sharing, no cycles)
    if (j2 === r1.head || j2 === j1 || r1.fastBlocks.has(j2) || r1.slowSet.has(j2)) return false;
    if (r2.slowEntry === r1.slowEntry) return false;
    for (const b of r2.fastBlocks)
        if (r1.fastBlocks.has(b) || r1.slowSet.has(b) || b === r1.head) return false;
    for (const b of r2.slowChain)
        if (r1.fastBlocks.has(b) || r1.slowSet.has(b) || b === r1.head) return false;

    // j2's predecessors must be exactly region2's exits (routing fills
    // every edge; a foreign edge would get an undominated value)
    for (const e of j2.predEdges) {
        const src = e.inst.block!;
        if (!r2.fastBlocks.has(src) && !r2.slowSet.has(src)) return false;
    }

    // j1's instruction shape: [effect-free prefix..., guard, cond_br]
    const term = j1.terminator!;
    const guard = term.operands[0]!;
    let prefixEnd = j1.insts.length - 1;
    if (guard.block === j1) {
        if (j1.insts[j1.insts.length - 2] !== guard) return false;
        prefixEnd = j1.insts.length - 2;
        // the guard may only feed this cond_br (an extra use would need
        // slow-side routing of a raw i1 — not a shape we build)
        let extraUse = false;
        fn.forEachInst((inst) => {
            if (inst === term) return;
            for (const o of inst.operands) if (o === guard) extraUse = true;
            if (inst.targets)
                for (const t of inst.targets) for (const a of t.args) if (a === guard) extraUse = true;
        });
        if (extraUse) return false;
    }
    const prefix: Inst[] = [];
    for (let i = 0; i < prefixEnd; i++) {
        const q = j1.insts[i]!;
        if (q.targets && q.targets.length > 0) return false;
        if (opInfo(q.op).effects !== Effect.NONE) return false;
        prefix.push(q);
    }

    // re-execution check: region2's guard failures jump to r1.slowEntry,
    // re-running r1's slow chain after r1's fast side already ran.  Every
    // instruction there must be effect-free or a whitelisted generic op
    // whose operands are proven numbers at ALL of r1's fast exits (the
    // only ways into region2's guards).
    for (const sb of r1.slowChain) {
        for (const inst of sb.insts) {
            if (inst.op === "br") continue;
            if (SLOW_OPS.has(inst.op)) {
                for (const o of inst.operands) {
                    for (const fe of r1.fastExitEdges) {
                        if (!provenNumberAt(o, fe.inst.block!, idom)) return false;
                    }
                }
            } else if (opInfo(inst.op).effects !== Effect.NONE) {
                return false;
            }
        }
    }

    // what the slow path knows each J1-defined value to be
    const slowMap = new Map<Inst, Inst>();
    const exitTarget = r1.slowExitEdge.inst.targets![r1.slowExitEdge.targetIndex]!;
    for (const p of j1.params) {
        const arg = exitTarget.args[j1.argIndexOfParam(p)];
        if (!arg) return false;
        slowMap.set(p, arg);
    }

    // routing pre-check: every use of a J1-defined value outside region2
    // must be dominated by j2 (it gets a routed param there)
    const routed: Inst[] = [...j1.params, ...prefix];
    // per value: uses that need the routed param / the slow substitute
    const outsideUses = new Map<Inst, Inst[]>(); // value -> using insts
    for (const v of routed) {
        const outs: Inst[] = [];
        let ok = true;
        fn.forEachInst((inst, blk) => {
            if (!ok) return;
            let uses = false;
            for (const o of inst.operands) if (o === v) uses = true;
            if (inst.targets)
                for (const t of inst.targets) for (const a of t.args) if (a === v) uses = true;
            if (!uses) return;
            if (blk === j1 || r2.fastBlocks.has(blk)) return; // stays valid (j1 dominates)
            if (r2.slowSet.has(blk)) return; // substituted below
            if (!dominates(idom, j2, blk)) {
                ok = false; // e.g. a catch handler outside the region
                return;
            }
            outs.push(inst);
        });
        if (!ok) return false;
        if (outs.length > 0) outsideUses.set(v, outs);
    }

    // ---- all checks passed; mutate ----
    const mapSlow = (v: Inst): Inst => slowMap.get(v) ?? v;

    // clone the pure prefix into r1's slow exit block so the slow chain
    // (and routing) can see those values
    const slowExitBlock = r1.slowChain[r1.slowChain.length - 1]!;
    const exitInst = r1.slowExitEdge.inst;
    for (const q of prefix) {
        const clone = new Inst(fn, q.op, q.operands.map(mapSlow), { ...q.imms });
        clone.block = slowExitBlock;
        slowExitBlock.insts.splice(slowExitBlock.insts.indexOf(exitInst), 0, clone);
        slowMap.set(q, clone);
    }

    // r1's slow path now falls through into r2's slow chain: the single
    // merged slow path is the full generic computation in program order
    retargetEdge(exitInst, r1.slowExitEdge.targetIndex, r2.slowEntry, []);
    // r2's guard failures re-enter the merged slow path from the top
    for (const ge of r2.guardFalseEdges) retargetEdge(ge.inst, ge.targetIndex, r1.slowEntry, []);
    // r2's slow chain computes on the slow-side values
    for (const sb of r2.slowChain) {
        for (const inst of sb.insts) {
            for (let i = 0; i < inst.operands.length; i++)
                inst.operands[i] = mapSlow(inst.operands[i]!);
            if (inst.targets)
                for (const t of inst.targets)
                    for (let i = 0; i < t.args.length; i++)
                        if (t.args[i]) t.args[i] = mapSlow(t.args[i]!);
        }
    }

    // route J1-defined values still used beyond region2 through j2
    for (const entry of outsideUses.entries()) {
        const v = entry[0];
        const users = entry[1];
        const vr = j2.addParam(v.nameHint);
        vr.type = v.type;
        const slot = j2.argIndexOfParam(vr);
        for (const e of j2.predEdges) {
            const t = e.inst.targets![e.targetIndex]!;
            t.args[slot] = r2.slowSet.has(e.inst.block!) ? mapSlow(v) : v;
        }
        for (const u of users) {
            for (let i = 0; i < u.operands.length; i++) if (u.operands[i] === v) u.operands[i] = vr;
            if (u.targets)
                for (const t of u.targets)
                    for (let i = 0; i < t.args.length; i++) if (t.args[i] === v) t.args[i] = vr;
        }
    }

    stats.regions_merged++;
    return true;
}

// --- pass (b): raw f64 params for optimizer-rewired joins -------------------

export function rawJoinParams(fn: Func, stats: OptStats): boolean {
    // candidates: non-entry, non-catch params whose every incoming arg is
    // a box_f64, an f64 value, itself, or another candidate param
    const cands = new Set<Inst>();
    for (const b of fn.blocks) {
        if (b.isCatch || b === fn.entry) continue;
        if (b.predEdges.length === 0) continue;
        for (const p of b.params) {
            if (p.isException || p.type !== "any") continue;
            let ok = true;
            for (const e of b.predEdges) {
                const t = e.inst.targets![e.targetIndex]!;
                if (t.kind === "unwind") {
                    ok = false;
                    break;
                }
                const arg = t.args[b.argIndexOfParam(p)];
                if (!arg) {
                    ok = false;
                    break;
                }
                if (arg === p || arg.op === "box_f64" || arg.type === "f64") continue;
                if (arg.op === "blockparam" && !arg.isException) continue; // resolved in pruning
                ok = false;
                break;
            }
            if (ok) cands.add(p);
        }
    }
    if (cands.size === 0) return false;

    // uses of every box_f64 that feeds a candidate (for the strip check)
    const boxUses = new Map<Inst, { inst: Inst; opIndex: number }[]>();
    fn.forEachInst((inst) => {
        const record = (v: Inst, opIndex: number) => {
            if (v.op !== "box_f64") return;
            const list = boxUses.get(v);
            if (list) list.push({ inst: inst, opIndex: opIndex });
            else boxUses.set(v, [{ inst: inst, opIndex: opIndex }]);
        };
        for (let i = 0; i < inst.operands.length; i++) record(inst.operands[i]!, i);
        if (inst.targets)
            for (const t of inst.targets) for (const a of t.args) if (a) record(a, -1);
    });

    // params fed by an edge-arg use of value v (empty if any use is not
    // an edge arg)
    const paramsFedBy = (v: Inst): Inst[] | null => {
        const fed: Inst[] = [];
        for (const u of boxUses.get(v) || []) {
            if (u.opIndex !== -1) return null; // consumed as an operand
            for (const t of u.inst.targets!) {
                for (let i = 0; i < t.args.length; i++) {
                    if (t.args[i] !== v) continue;
                    const p = t.block.params[i + (t.block.isCatch ? 1 : 0)];
                    if (!p) return null;
                    fed.push(p);
                }
            }
        }
        return fed;
    };

    // prune to a fixpoint.  Two conditions:
    //  - every arg is admissible (box_f64 strippable / f64 / candidate);
    //  - the candidate is ROOTED: some arg chain reaches an actual f64
    //    producer.  A cycle of params feeding only each other must not
    //    self-justify — there would be no f64 anywhere in it (the
    //    verifier would reject the result; refuse it here instead).
    let pruned = true;
    while (pruned) {
        pruned = false;
        for (const p of cands) {
            const b = p.block!;
            const argIdx = b.argIndexOfParam(p);
            let keep = true;
            for (const e of b.predEdges) {
                const arg = e.inst.targets![e.targetIndex]!.args[argIdx]!;
                if (arg === p || arg.type === "f64") continue;
                if (arg.op === "blockparam") {
                    if (!cands.has(arg)) keep = false;
                } else if (arg.op === "box_f64") {
                    // stripping the box must leave it dead: every use an
                    // edge arg into a candidate param
                    const fed = paramsFedBy(arg);
                    if (!fed || !fed.every((fp) => cands.has(fp) || fp.type === "f64"))
                        keep = false;
                }
                if (!keep) break;
            }
            if (!keep) {
                cands.delete(p);
                pruned = true;
            }
        }
        // rootedness: propagate from box_f64/f64 args through the
        // candidate graph; drop anything unreached
        const rooted = new Set<Inst>();
        let grew = true;
        while (grew) {
            grew = false;
            for (const p of cands) {
                if (rooted.has(p)) continue;
                const b = p.block!;
                const argIdx = b.argIndexOfParam(p);
                for (const e of b.predEdges) {
                    const arg = e.inst.targets![e.targetIndex]!.args[argIdx]!;
                    if (
                        arg.op === "box_f64" ||
                        arg.type === "f64" ||
                        (arg.op === "blockparam" && rooted.has(arg))
                    ) {
                        rooted.add(p);
                        grew = true;
                        break;
                    }
                }
            }
        }
        for (const p of cands) {
            if (!rooted.has(p)) {
                cands.delete(p);
                pruned = true;
            }
        }
    }
    if (cands.size === 0) return false;

    // convert: retype params, strip boxes on the edges
    for (const p of cands) {
        p.type = "f64";
        p.rawJoin = true;
        stats.raw_join_params++;
        const b = p.block!;
        const argIdx = b.argIndexOfParam(p);
        for (const e of b.predEdges) {
            const t = e.inst.targets![e.targetIndex]!;
            const arg = t.args[argIdx]!;
            if (arg.op === "box_f64") t.args[argIdx] = arg.operands[0]!;
        }
    }

    // rewrite uses: unbox_f64(p) collapses to p; anything still needing
    // a boxed value re-boxes once at the head of p's block (the single
    // box at the region exit)
    for (const p of cands) {
        const b = p.block!;
        const unboxes: Inst[] = [];
        const boxedUsers: Inst[] = [];
        fn.forEachInst((inst) => {
            if (inst.op === "unbox_f64" && inst.operands[0] === p) {
                if (!inst.targets || inst.targets.length === 0) unboxes.push(inst);
                return;
            }
            let boxedUse = false;
            const info = opInfo(inst.op);
            inst.operands.forEach((o, i) => {
                if (o !== p) return;
                const want = info.sig ? info.sig.params[i] : undefined;
                if (want !== "f64") boxedUse = true;
            });
            if (inst.targets) {
                for (const t of inst.targets) {
                    t.args.forEach((a, i) => {
                        if (a !== p) return;
                        const tp = t.block.params[i + (t.block.isCatch ? 1 : 0)];
                        if (!tp || tp.type !== "f64") boxedUse = true;
                    });
                }
            }
            if (boxedUse) boxedUsers.push(inst);
        });
        for (const u of unboxes) {
            // u's consumers take f64: p is one now
            fn.forEachInst((inst) => {
                for (let i = 0; i < inst.operands.length; i++)
                    if (inst.operands[i] === u) inst.operands[i] = p;
                if (inst.targets)
                    for (const t of inst.targets)
                        for (let i = 0; i < t.args.length; i++) if (t.args[i] === u) t.args[i] = p;
            });
            const ub = u.block!;
            ub.insts.splice(ub.insts.indexOf(u), 1);
            u.block = null;
        }
        if (boxedUsers.length > 0) {
            const nb = new Inst(fn, "box_f64", [p], {});
            nb.block = b;
            b.insts.unshift(nb);
            for (const u of boxedUsers) {
                const info = opInfo(u.op);
                u.operands.forEach((o, i) => {
                    if (o !== p) return;
                    const want = info.sig ? info.sig.params[i] : undefined;
                    if (want !== "f64") u.operands[i] = nb;
                });
                if (u.targets) {
                    for (const t of u.targets) {
                        t.args.forEach((a, i) => {
                            if (a !== p) return;
                            const tp = t.block.params[i + (t.block.isCatch ? 1 : 0)];
                            if (!tp || tp.type !== "f64") t.args[i] = nb;
                        });
                    }
                }
            }
        }
    }
    return true;
}

// --- driver -----------------------------------------------------------------

// run guard folding + region merging to a fixpoint.  Cheap bail when the
// function has no number guards (every flag-off compile).
export function optimizeGuardRegions(fn: Func, stats: OptStats): boolean {
    let hasGuard = false;
    for (const b of fn.blocks) {
        const t = b.terminator;
        if (t && t.op === "cond_br" && isNumberGuard(t.operands[0]!)) {
            hasGuard = true;
            break;
        }
    }
    if (!hasGuard) return false;

    // drop builder-era unreachable blocks up front: region matching and
    // the routing dominance checks assume every block in fn.blocks is
    // reachable (flag-off compiles bailed above and stay byte-pure)
    sweepUnreachableBlocks(fn);

    let changedAny = false;
    for (let round = 0; round < 50; round++) {
        let changed = false;
        // merge FIRST, fold after: folding an adjacent diamond's guards
        // early dissolves its slow path and leaves a mixed fast/slow
        // join in the middle of what should become one region — the
        // merged fast side would keep box/unbox round-trips.  Merging
        // needs no folding to match (interior guard steps and dup
        // guards are part of the recognized shape).
        for (let merges = 0; merges < 50; merges++) {
            const { rpo } = computeRPO(fn);
            const idom = computeDominators(fn, rpo);
            let merged = false;
            for (const b of rpo) {
                const r1 = matchRegionAt(b);
                if (!r1) continue;
                if (tryMergeAt(fn, r1, idom, stats)) {
                    merged = true;
                    changed = true;
                    break; // mutations invalidate matches; re-match
                }
            }
            if (!merged) break;
        }
        if (foldProvenGuards(fn, stats)) changed = true;
        if (!changed) break;
        sweepUnreachableBlocks(fn);
        changedAny = true;
    }
    return changedAny;
}
