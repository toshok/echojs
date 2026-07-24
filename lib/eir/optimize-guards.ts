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
//   - J1's predecessors must be EXACTLY R1's exits and J2's exactly
//     R2's: a foreign edge into either join would make the substituted
//     slow values wrong (J1) or undominated (J2) on the foreign path.
//   - BOTH regions' slow chains must be the GENERIC TWIN of their fast
//     sides (verifyGenericTwin, applied symmetrically): same arithmetic
//     ops in the same order with corresponding operands and
//     corresponding join-exit arguments.  R2's twin-ness covers the
//     R1-slow route that would have taken R2's fast arm; R1's twin-ness
//     covers the mirrored route — R2 guard failures after R1's fast arm
//     ran, rerouted through R1's slow chain (whose exit values then
//     substitute into R2's slow ops).  The re-execution purity check
//     proves those detours unobservable; twin-ness is what proves their
//     VALUES agree with the fast side.  Nothing about either arm is
//     assumed anymore — both are verified.
//   - values defined at J1 (params + the pure instruction prefix ahead
//     of R2's guard) that are still used beyond R2 are routed through
//     R2's join as new params — fast edges pass the J1 value, the slow
//     edge passes its slow-side substitute — after checking that every
//     such use IS dominated by that join (else the merge is refused);
//     raw-typed (i1/f64) values are never routed — merge refused
//     (fail-closed).
//
// Raw f64 joins (pass b): a param is converted only when every incoming
// argument is a box_f64 result (whose only consumers are edges feeding
// converted params), an f64 value, or another converted param.  The
// boxes are stripped on the edges, unbox_f64 uses collapse to the param
// itself, and any remaining boxed use re-boxes ONCE at the head of the
// param's block — that is the single box at the region exit.  The
// verifier re-checks all of it (see verifier.ts rawJoin rules).

import { Func, Block, Inst } from "./ir";
import type { Module, ShapeField, Target } from "./ir";
import { Effect, opInfo } from "./ops";
import {
    computeRPO,
    computeDominators,
    dominates,
    computeShapeFacts,
    shapeFactKey,
} from "./verifier";
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

// EIR f64 op -> its generic twin
const F64_TO_GENERIC: Record<string, string | undefined> = {
    f64_add: "add",
    f64_sub: "sub",
    f64_mul: "mul",
    f64_div: "div",
    f64_lt: "lt",
};

// Verify that region2's slow chain is the generic rendition of its fast
// side: the same arithmetic ops in the same order, with operands that
// correspond under the box/unbox mapping, and join-exit arguments that
// correspond slot for slot.  On number inputs a generic op is pure and
// bit-identical to its f64 twin, so this is exactly the condition under
// which rerouting a would-have-taken-the-fast-arm execution through the
// slow chain preserves behavior.  Anything unrecognized refuses.
//
// Correspondence rules (fast value -> the slow value it must equal):
//   unbox_f64(x)              -> slowOf(x)
//   earlier paired f64 op     -> that op's slow twin's result
// where slowOf(x):
//   box_f64(f)                        -> f's rule above
//   param of an interior fast block   -> slowOf(its single incoming arg)
//   j1 params / anything else         -> x itself (the slow chain sees
//     the same SSA value; the merge's sigma rewrites j1 params later)
//
// f64_lt (and hence const-boolean split arms) is refused — the check
// runs on BOTH sides of a merge, so lt regions simply do not merge at
// all; the boolean-twin correspondence would add checking surface for
// shapes with no measured benefit (hypot2/bench stats unaffected).
function verifyGenericTwin(r2: GuardRegion): boolean {
    if (r2.fastExitEdges.length !== 1) return false; // lt splits etc.

    const slowOps: Inst[] = [];
    for (const sb of r2.slowChain)
        for (const inst of sb.insts) if (SLOW_OPS.has(inst.op)) slowOps.push(inst);

    const pair = new Map<Inst, Inst>(); // fast f64 op -> slow twin

    // correspondence is SSA identity, with one extension: two const
    // instructions with the same kind/value are the same value on every
    // path (the merge clones pure prefix consts into the slow chain, so
    // an earlier merge's region legitimately references the clone where
    // the fast side references the original).  Value equality must be
    // Object.is, not ===: `0 === -0` would conflate the two zeros (a
    // sign flip observable via 1/x — review attack H), while NaN
    // consts — which === would needlessly refuse — all denote the one
    // JS NaN and correspond.
    const corresponds = (want: Inst, actual: Inst | null | undefined): boolean => {
        if (!actual) return false;
        if (want === actual) return true;
        return (
            want.op === "const" &&
            actual.op === "const" &&
            want.imms["kind"] === actual.imms["kind"] &&
            Object.is(want.imms["value"], actual.imms["value"])
        );
    };

    const slowOfBoxed = (x: Inst, d: number): Inst | null => {
        if (d <= 0) return null;
        if (x.op === "box_f64") return slowOfF64(x.operands[0]!, d - 1);
        if (x.op === "blockparam" && x.block && r2.fastBlocks.has(x.block)) {
            const b = x.block;
            if (b.predEdges.length !== 1) return null;
            const e = b.predEdges[0]!;
            const arg = e.inst.targets![e.targetIndex]!.args[b.argIndexOfParam(x)];
            return arg ? slowOfBoxed(arg, d - 1) : null;
        }
        return x;
    };
    const slowOfF64 = (f: Inst, d: number): Inst | null => {
        if (d <= 0) return null;
        if (f.op === "unbox_f64") return slowOfBoxed(f.operands[0]!, d - 1);
        return pair.get(f) ?? null; // must be an already-paired f64 op
    };

    // linear walk of the fast side (single path: guards have one fast
    // arm, lt splits are refused above), pairing arithmetic in order
    let k = 0;
    const seen = new Set<Block>();
    let b: Block | null = r2.head.terminator!.targets![0]!.block;
    let exitArgs: (Inst | null)[] | null = null;
    while (b) {
        if (b === r2.join || seen.has(b) || !r2.fastBlocks.has(b)) return false;
        seen.add(b);
        const t: Inst = b.terminator!;
        for (const inst of b.insts) {
            if (inst === t) break;
            const gop = F64_TO_GENERIC[inst.op];
            if (!gop) continue; // unbox/box/const/has_tag: no twin needed
            if (inst.op === "f64_lt") return false;
            if (k >= slowOps.length) return false;
            const tw = slowOps[k++]!;
            if (tw.op !== gop) return false;
            for (let i = 0; i < inst.operands.length; i++) {
                const want = slowOfF64(inst.operands[i]!, 32);
                if (!want || !corresponds(want, tw.operands[i])) return false;
            }
            pair.set(inst, tw);
        }
        if (t.op === "br") {
            const tg: Target = t.targets![0]!;
            if (tg.block === r2.join) {
                exitArgs = tg.args;
                b = null;
            } else b = tg.block;
        } else if (t.op === "cond_br" && isNumberGuard(t.operands[0]!)) {
            b = t.targets![0]!.block;
        } else {
            return false;
        }
    }
    if (!exitArgs || k !== slowOps.length) return false;

    // join-exit correspondence: what flows out of the fast arm must be
    // what flows out of the slow chain, slot for slot
    const slowExitArgs = r2.slowExitEdge.inst.targets![r2.slowExitEdge.targetIndex]!.args;
    if (exitArgs.length !== slowExitArgs.length) return false;
    for (let i = 0; i < exitArgs.length; i++) {
        const fa = exitArgs[i];
        const sa = slowExitArgs[i];
        if (!fa || !sa) return false;
        const want = slowOfBoxed(fa, 32);
        if (!want || !corresponds(want, sa)) return false;
    }
    return true;
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

    // j1's predecessors must be exactly region1's exits.  A foreign edge
    // into j1 means region2's guards are reachable WITHOUT region1
    // having run; the slow-side substitution below would then hand
    // region2's slow chain region1's slow values, which hold garbage on
    // the foreign path (review attack A).
    for (const e of j1.predEdges) {
        const src = e.inst.block!;
        if (!r1.fastBlocks.has(src) && !r1.slowSet.has(src)) return false;
    }
    // j2's predecessors must be exactly region2's exits (routing fills
    // every edge; a foreign edge would get an undominated value)
    for (const e of j2.predEdges) {
        const src = e.inst.block!;
        if (!r2.fastBlocks.has(src) && !r2.slowSet.has(src)) return false;
    }

    // BOTH regions' slow chains must be the GENERIC TWIN of their fast
    // sides.  Region2: the merge reroutes region1's slow exit straight
    // into region2's slow chain — including executions where region2's
    // guards would have PASSED pre-merge (e.g. a guard on a mul result,
    // which is always a number) and run the fast arm (review attack F).
    // Region1, the exact mirror (review attack G): region2's guard
    // failures — which happen after region1's FAST side ran — are
    // rerouted through region1's slow chain, and region2's slow chain
    // is rewritten against region1's SLOW values; the purity
    // (re-execution) check below proves that detour unobservable, but
    // only twin-ness makes its VALUES identical to what the fast side
    // already produced.
    if (!verifyGenericTwin(r2)) return false;
    if (!verifyGenericTwin(r1)) return false;

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
        if (outs.length > 0) {
            // routed params are ordinary boxed joins; a RAW-typed j1
            // value (an i1/f64 prefix inst) live past j2 would need a
            // raw param this pass has no business minting — refuse the
            // merge (fail-closed by design: the verifier would reject
            // the result anyway, we just decline up front)
            if (v.type !== "any") return false;
            outsideUses.set(v, outs);
        }
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
    // a box_f64, an f64 value, a number constant (Phase 3.6: converted to
    // a raw f64_const on the edge — a loop accumulator seeded `x = 0`
    // now qualifies), itself, or another candidate param
    const isNumConst = (v: Inst) => v.op === "const" && v.imms["kind"] === "number";
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
                if (isNumConst(arg)) continue;
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
                if (arg === p || arg.type === "f64" || isNumConst(arg)) continue;
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
                    // NB: a number const is admissible but NOT a root — a
                    // const-only join must stay boxed (flag-off code would
                    // otherwise grow boxes for no typed-region payoff);
                    // only a real f64/box_f64 producer roots the graph.
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
            else if (isNumConst(arg)) {
                // mint the raw producer on the edge; the boxed const keeps
                // its other users and falls to DCE when this was the last
                const fc = new Inst(fn, "f64_const", [], { value: arg.imms["value"] });
                const eb = e.inst.block!;
                fc.block = eb;
                eb.insts.splice(eb.insts.indexOf(e.inst), 0, fc);
                t.args[argIdx] = fc;
            }
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

// --- boolean-join threading ---------------------------------------------------

// A comparison that rejoins as boxed booleans and immediately re-tests:
//
//     ^t: br -> ^join(const true)      ^f: br -> ^join(const false)
//     ^join(%p): %b = to_boolean %p; cond_br %b -> ^then, ^else
//
// threads each constant edge straight to the cond_br successor it would
// pick (to_boolean(const true/false) is exact), so the fast arm of an
// f64_lt diamond — and a Phase 3.6 clone's trusted compare — branches on
// the raw i1 with no boxed-boolean round-trip (and no _ejs_truthy call)
// left in the loop.  Trust-free: constants only.  Non-constant edges (a
// diamond's generic slow arm) keep the join and the re-test.
export function threadBooleanJoins(fn: Func, stats: OptStats): boolean {
    // uses of every value (operands + outgoing edge args), for the
    // locality check below
    const useCount = new Map<Inst, number>();
    const bump = (v: Inst) => useCount.set(v, (useCount.get(v) || 0) + 1);
    fn.forEachInst((inst) => {
        for (const o of inst.operands) bump(o);
        if (inst.targets) for (const t of inst.targets) for (const a of t.args) if (a) bump(a);
    });

    let changed = false;
    for (const b of fn.blocks) {
        if (b.isCatch || b === fn.entry) continue;
        if (b.params.length !== 1 || b.insts.length !== 2) continue;
        const p = b.params[0]!;
        if (p.isException || p.removed) continue;
        const tob = b.insts[0]!;
        const br = b.insts[1]!;
        if (tob.op !== "to_boolean" || tob.operands[0] !== p) continue;
        if (br.op !== "cond_br" || br.operands[0] !== tob) continue;
        // the join's OWN definitions must die inside it: a use of the
        // param (or the boolean) downstream would lose def-dominates-use
        // the moment an edge bypasses the block
        if (useCount.get(p) !== 1 || useCount.get(tob) !== 1) continue;
        if (!br.targets || br.targets.length !== 2) continue;
        const tTrue = br.targets[0]!;
        const tFalse = br.targets[1]!;
        if (tTrue.block === b || tFalse.block === b) continue;
        if (tTrue.args.length !== 0 || tFalse.args.length !== 0) continue;

        // predEdges mutate as edges retarget: snapshot first
        for (const e of b.predEdges.slice()) {
            const t = e.inst.targets![e.targetIndex]!;
            if (t.kind === "unwind") continue;
            const arg = t.args[0];
            if (!arg || arg.op !== "const" || arg.imms["kind"] !== "boolean") continue;
            const dest = arg.imms["value"] ? tTrue.block : tFalse.block;
            retargetEdge(e.inst, e.targetIndex, dest, []);
            stats.joins_threaded++;
            changed = true;
        }
    }
    if (changed) sweepUnreachableBlocks(fn);
    return changed;
}

// --- shapes-plan P4.3: shape-guard regions ------------------------------------
//
// The shape twins of pass (a): consecutive GET diamonds on the same
// receiver and shape merge into one guard region with one slow path, and
// guards proven by an un-killed dominating shape fact fold.  All facts
// come from verifier.ts's computeShapeFacts — the same engine the
// verifier re-checks the result with, so a fold or merge this pass gets
// wrong is IR the verifier rejects (trust-free, the P3.4 discipline).
//
// ---- Soundness inventory (the shape additions) ----
//
//   - Fact folding: a cond_br on has_shape(v, S) rewrites to br(true)
//     when the fact (v, S) holds at the branch.  Facts only enter blocks
//     on guard edges and die at WRITE|CALL instructions (the effect-kill
//     rule — see verifier.ts), so a held fact means the header compare
//     provably answers true.  Folding removes CFG edges only; a stale
//     (pre-fold) fact analysis is conservative, and the fact continues to
//     reach the true target THROUGH the folded block (its instructions
//     are kill-free on that path, or the fact would not have held).
//   - Region shape (matchShapeRegionAt): head ends in cond_br on
//     has_shape(recv, S); the fast side is a LINEAR br chain whose
//     instructions are effect-free-or-GC plus slot_loads on exactly
//     (recv, S); the slow side is the numeric matcher's linear chain with
//     get_prop_atom(recv) as the one effectful op.  Anything else — a
//     store diamond's has_tag split, an interior guard, a foreign edge —
//     refuses the match (fail-closed).
//   - Merging (tryMergeShapeAt, the numeric merge transplanted):
//     region2's guard failures reroute to region1's slow entry, which
//     RE-EXECUTES region1's slow chain after region1's fast side already
//     ran.  That is sound because (a) the fast side and j1 prefix are
//     kill-free, so the receiver still has shape S there, and (b) every
//     re-executed get_prop_atom names a field OF S — a get of an own
//     plain data property: no getter, no proto walk, no effects, and
//     bit-identical to the slot_load the fast side already did.  The
//     TWIN check (verifyShapeTwin) is what proves (b) plus the pairing:
//     fast slot_loads and slow gets correspond op for op (atom == the
//     shape's field name at that slot, receiver == recv on both sides)
//     and join-exit args correspond slot for slot — both regions are
//     checked, exactly like the numeric merge's symmetric twin rule.
//   - Everything else (j1/j2 pred exactness, pure-prefix cloning, routing
//     of j1-defined values through j2 with raw-type refusal) is the
//     numeric merge's argument verbatim.
//
// ---- P4.5 typed slots: the mixed region and the heterogeneous merge ----
//
//   - An f64-repr slot_load produces a raw f64 and lowering boxes it at
//     the fast exit, so a shape region's fast side now also carries
//     box_f64/unbox_f64 and — after a merge — the f64 arithmetic the
//     numeric machinery moved in.  The shape matcher therefore admits the
//     numeric whitelist in its SLOW chain too (the generic ops are the
//     slow rendition of that arithmetic), and the twin check pairs BOTH
//     populations: slot_loads with gets (atom == field-at-slot, the P4.3
//     rule) and f64 ops with generic ops (operand correspondence through
//     the box/unbox mapping, the numeric rule verbatim).  A box_f64 of an
//     f64 slot_load corresponds to that load's paired get: the NaN-box
//     stores doubles raw, so the get returns bit-for-bit the boxed form
//     of the double the load produced.
//   - tryMergeShapeNumericAt (the heterogeneous merge): a NUMERIC region
//     headed at a shape region's join merges into it — r2's has_tag
//     failures reroute to r1's slow entry exactly like a second shape
//     region's guard failures would.  After the merge r2's head params are
//     fed only by r1's fast exits (all box_f64), so foldProvenGuards
//     deletes the has_tag and rawJoinParams turns the join raw: the
//     region computes unboxed end-to-end, which is the entire point.
//   - Re-executing r1's slow chain may now re-run generic arithmetic.
//     Sound when each operand is either proven-number at r1's fast exit
//     (the numeric merge's rule) or the result of one of r1's own paired
//     gets naming an f64-REPR field of the guarded shape: the receiver
//     still has shape S (kill-free fast side), an f64-repr slot holds a
//     number by the shaped-world invariant, so the get returns a number
//     and the generic op is pure and bit-identical to its f64 twin.

interface ShapeRegion {
    head: Block;
    recv: Inst; // the guarded receiver value
    shapeKey: string; // imms.shape of the head guard
    fastBlocks: Set<Block>;
    fastChain: Block[]; // linear br chain, entry..exit
    fastLoads: Inst[]; // slot_loads in chain order
    fastArith: Inst[]; // P4.5: f64 arithmetic in chain order (post-merge)
    fastExitEdge: EdgeRef;
    slowEntry: Block;
    slowChain: Block[];
    slowSet: Set<Block>;
    slowGets: Inst[]; // get_prop_atom in chain order
    slowArith: Inst[]; // P4.5: whitelisted generic ops in chain order
    slowExitEdge: EdgeRef;
    join: Block;
}

// structurally verify the shape-get region headed at `head`; null on any
// deviation.  Strictly linear on both sides (see the inventory above).
function matchShapeRegionAt(head: Block): ShapeRegion | null {
    const term = head.terminator;
    if (!term || term.op !== "cond_br") return null;
    const cond = term.operands[0]!;
    if (cond.op !== "has_shape") return null;
    const recv = cond.operands[0]!;
    const shapeKey = String(cond.imms["shape"]);
    const t0 = term.targets![0]!;
    const t1 = term.targets![1]!;
    if (t0.args.length !== 0 || t1.args.length !== 0) return null;
    const slowEntry = t1.block;
    if (slowEntry.isCatch || t0.block.isCatch) return null;
    if (slowEntry.params.length !== 0) return null;
    if (t0.block === slowEntry) return null;

    // --- slow side: the numeric matcher's linear chain, with
    // get_prop_atom(recv) — and, P4.5, the numeric whitelist ops (the
    // generic rendition of merged-in f64 arithmetic) — as the admitted
    // effectful ops
    const slowChain: Block[] = [];
    const slowSet = new Set<Block>();
    const slowGets: Inst[] = [];
    const slowArith: Inst[] = [];
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
            if (inst.op === "get_prop_atom") {
                if (inst.operands[0] !== recv) return null;
                slowGets.push(inst);
            } else if (SLOW_OPS.has(inst.op)) {
                slowArith.push(inst);
            } else if (opInfo(inst.op).effects !== Effect.NONE) {
                return null;
            }
        }
        let exit: EdgeRef;
        if (bt.op === "br") {
            exit = { inst: bt, targetIndex: 0 };
        } else if (
            bt.op === "get_prop_atom" &&
            bt.targets &&
            bt.targets.length === 2 &&
            bt.targets[0]!.kind === "normal"
        ) {
            // a get inside a protected region: [normal, unwind]
            if (bt.operands[0] !== recv) return null;
            slowGets.push(bt);
            exit = { inst: bt, targetIndex: 0 };
        } else if (
            SLOW_OPS.has(bt.op) &&
            bt.targets &&
            bt.targets.length === 2 &&
            bt.targets[0]!.kind === "normal"
        ) {
            // a generic op inside a protected region: [normal, unwind]
            slowArith.push(bt);
            exit = { inst: bt, targetIndex: 0 };
        } else {
            return null;
        }
        const next = exit.inst.targets![exit.targetIndex]!.block;
        if (next.isCatch) return null;
        if (next.predEdges.every((e) => slowSet.has(e.inst.block!))) {
            sb = next;
            continue;
        }
        join = next;
        slowExitEdge = exit;
        break;
    }
    if (!join || join.isCatch || join === head) return null;

    // --- fast side: a linear br chain of effect-free-or-GC instructions
    // plus slot_loads on exactly (recv, shapeKey); f64 arithmetic (an
    // earlier heterogeneous merge's residue) is collected for the twin
    const fastBlocks = new Set<Block>();
    const fastChain: Block[] = [];
    const fastLoads: Inst[] = [];
    const fastArith: Inst[] = [];
    let fastExitEdge: EdgeRef | null = null;
    let fb: Block | null = t0.block;
    while (fb) {
        if (fastBlocks.has(fb)) return null;
        if (fastBlocks.size > MAX_REGION_BLOCKS) return null;
        if (fb === join || fb === head || slowSet.has(fb) || fb.isCatch) return null;
        fastBlocks.add(fb);
        fastChain.push(fb);
        const ft = fb.terminator;
        if (!ft || ft.op !== "br") return null; // strictly linear
        for (const inst of fb.insts) {
            if (inst === ft) continue;
            if (inst.targets && inst.targets.length > 0) return null;
            if (inst.op === "slot_load") {
                if (inst.operands[0] !== recv) return null;
                if (String(inst.imms["shape"]) !== shapeKey) return null;
                fastLoads.push(inst);
            } else if (F64_TO_GENERIC[inst.op]) {
                fastArith.push(inst);
            } else if ((opInfo(inst.op).effects & ~Effect.GC) !== 0) {
                return null;
            }
        }
        const tg: Target = ft.targets![0]!;
        if (tg.block === join) {
            fastExitEdge = { inst: ft, targetIndex: 0 };
            fb = null;
        } else {
            if (tg.args.length !== 0 && tg.block.params.length === 0) return null;
            fb = tg.block;
        }
    }
    if (!fastExitEdge) return null;
    // the fast side is entered only through the head's guard
    for (const b of fastBlocks) {
        for (const e of b.predEdges) {
            const src = e.inst.block!;
            if (src !== head && !fastBlocks.has(src)) return null;
        }
    }

    return {
        head,
        recv,
        shapeKey,
        fastBlocks,
        fastChain,
        fastLoads,
        fastArith,
        fastExitEdge,
        slowEntry,
        slowChain,
        slowSet,
        slowGets,
        slowArith,
        slowExitEdge: slowExitEdge!,
        join,
    };
}

// the slow chain is the generic rendition of the fast side: slot_loads and
// gets pair op for op (atom == the shape's field at that slot), f64
// arithmetic and generic ops pair op for op with corresponding operands
// (P4.5, the numeric twin rule), and the join-exit arguments correspond
// slot for slot.  A box_f64 of an f64 slot_load corresponds to the load's
// paired get: doubles are stored raw in the NaN-box, so the get returns
// exactly the boxed rendition of the load's raw double.
function verifyShapeTwin(r: ShapeRegion, shapes: Map<string, ShapeField[]>): boolean {
    const fields = shapes.get(r.shapeKey);
    if (!fields) return false;
    if (r.fastLoads.length !== r.slowGets.length) return false;
    if (r.fastArith.length !== r.slowArith.length) return false;
    const pair = new Map<Inst, Inst>(); // fast load/arith -> slow twin
    for (let i = 0; i < r.fastLoads.length; i++) {
        const load = r.fastLoads[i]!;
        const get = r.slowGets[i]!;
        const slot = load.imms["slot"] as number;
        if (typeof slot !== "number" || slot < 0 || slot >= fields.length) return false;
        if (fields[slot]!.name !== get.imms["atom"]) return false;
        pair.set(load, get);
    }

    // const-correspondence, the numeric merge's Object.is rule, extended
    // to the raw form a prior rawJoin conversion mints on fast edges
    const corresponds = (want: Inst, actual: Inst): boolean => {
        if (want === actual) return true;
        if (
            want.op === "const" &&
            actual.op === "const" &&
            want.imms["kind"] === actual.imms["kind"] &&
            Object.is(want.imms["value"], actual.imms["value"])
        )
            return true;
        return (
            want.op === "f64_const" &&
            actual.op === "const" &&
            actual.imms["kind"] === "number" &&
            Object.is(want.imms["value"], actual.imms["value"])
        );
    };

    // fast value -> the slow value it must equal at the join.  Boxed and
    // raw views recurse into each other through box/unbox exactly as the
    // numeric twin's slowOfBoxed/slowOfF64 do, with slot_loads bottoming
    // out at their paired gets.
    const slowOf = (x: Inst, d: number): Inst | null => {
        if (d <= 0) return null;
        const p = pair.get(x);
        if (p) return p;
        if (x.op === "box_f64" || x.op === "unbox_f64") return slowOf(x.operands[0]!, d - 1);
        if (x.op === "blockparam" && x.block && r.fastBlocks.has(x.block)) {
            const b = x.block;
            if (b.predEdges.length !== 1) return null;
            const e = b.predEdges[0]!;
            const arg = e.inst.targets![e.targetIndex]!.args[b.argIndexOfParam(x)];
            return arg ? slowOf(arg, d - 1) : null;
        }
        return x; // defined above the head: the same SSA value on both sides
    };

    // pair the arithmetic in chain order with corresponding operands.
    // f64_lt is refused exactly as the numeric twin refuses it (the check
    // runs on both sides of a merge, so lt regions simply do not merge).
    for (let i = 0; i < r.fastArith.length; i++) {
        const fa = r.fastArith[i]!;
        const sa = r.slowArith[i]!;
        if (fa.op === "f64_lt") return false;
        if (F64_TO_GENERIC[fa.op] !== sa.op) return false;
        for (let k = 0; k < fa.operands.length; k++) {
            const want = slowOf(fa.operands[k]!, 32);
            if (!want || !corresponds(want, sa.operands[k]!)) return false;
        }
        pair.set(fa, sa);
    }

    const fastArgs = r.fastExitEdge.inst.targets![r.fastExitEdge.targetIndex]!.args;
    const slowArgs = r.slowExitEdge.inst.targets![r.slowExitEdge.targetIndex]!.args;
    if (fastArgs.length !== slowArgs.length) return false;
    for (let i = 0; i < fastArgs.length; i++) {
        const fa = fastArgs[i];
        const sa = slowArgs[i];
        if (!fa || !sa) return false;
        const want = slowOf(fa, 32);
        if (!want) return false;
        if (!corresponds(want, sa)) return false;
    }
    return true;
}

// Re-executing r1's slow chain (a merged region's guard failures reroute
// through it) is sound when every instruction is effect-free, a get of an
// own field of the guarded shape (pure and bit-identical while the
// receiver still has shape S — the fast side is kill-free), or (P4.5) a
// whitelisted generic op each of whose operands is proven-number at r1's
// fast exit or is one of r1's own paired gets naming an f64-REPR field —
// an f64 slot holds a number by the shaped-world invariant, so the
// re-executed generic op is pure and bit-identical to its f64 twin.
function checkShapeSlowReexec(
    r1: ShapeRegion,
    fields: ShapeField[],
    idom: Map<Block, Block>
): boolean {
    const fastExitBlock = r1.fastExitEdge.inst.block!;
    const numberOk = (o: Inst): boolean => {
        if (provenNumberAt(o, fastExitBlock, idom)) return true;
        if (o.op !== "get_prop_atom" || !r1.slowGets.includes(o)) return false;
        const f = fields.find((f) => f.name === o.imms["atom"]);
        return f !== undefined && f.repr === "f64";
    };
    for (const sb of r1.slowChain) {
        for (const inst of sb.insts) {
            if (inst.op === "br") continue;
            if (inst.op === "get_prop_atom") {
                if (inst.operands[0] !== r1.recv) return false;
                if (!fields.some((f) => f.name === inst.imms["atom"])) return false;
            } else if (SLOW_OPS.has(inst.op)) {
                for (const o of inst.operands) if (!numberOk(o)) return false;
            } else if (opInfo(inst.op).effects !== Effect.NONE) {
                return false;
            }
        }
    }
    return true;
}

// merge the shape region headed at r1.join (if any) into r1.  All checks
// precede all mutations — the numeric tryMergeAt transplanted.
function tryMergeShapeAt(
    fn: Func,
    shapes: Map<string, ShapeField[]>,
    r1: ShapeRegion,
    idom: Map<Block, Block>,
    stats: OptStats
): boolean {
    const j1 = r1.join;
    const r2 = matchShapeRegionAt(j1);
    if (!r2) return false;
    if (r2.recv !== r1.recv || r2.shapeKey !== r1.shapeKey) return false;
    const j2 = r2.join;

    // region2 strictly below region1 (no sharing, no cycles)
    if (j2 === r1.head || j2 === j1 || r1.fastBlocks.has(j2) || r1.slowSet.has(j2)) return false;
    if (r2.slowEntry === r1.slowEntry) return false;
    for (const b of r2.fastBlocks)
        if (r1.fastBlocks.has(b) || r1.slowSet.has(b) || b === r1.head) return false;
    for (const b of r2.slowChain)
        if (r1.fastBlocks.has(b) || r1.slowSet.has(b) || b === r1.head) return false;

    // j1's predecessors must be exactly region1's exits, j2's exactly
    // region2's (the numeric merge's review attack A)
    for (const e of j1.predEdges) {
        const src = e.inst.block!;
        if (!r1.fastBlocks.has(src) && !r1.slowSet.has(src)) return false;
    }
    for (const e of j2.predEdges) {
        const src = e.inst.block!;
        if (!r2.fastBlocks.has(src) && !r2.slowSet.has(src)) return false;
    }

    // both regions' slow chains must be their fast sides' generic twins
    if (!verifyShapeTwin(r2, shapes)) return false;
    if (!verifyShapeTwin(r1, shapes)) return false;

    // j1's instruction shape: [effect-free prefix..., guard, cond_br]
    const term = j1.terminator!;
    const guard = term.operands[0]!;
    let prefixEnd = j1.insts.length - 1;
    if (guard.block === j1) {
        if (j1.insts[j1.insts.length - 2] !== guard) return false;
        prefixEnd = j1.insts.length - 2;
        let extraUse = false;
        fn.forEachInst((inst) => {
            if (inst === term) return;
            for (const o of inst.operands) if (o === guard) extraUse = true;
            if (inst.targets)
                for (const t of inst.targets) for (const a of t.args) if (a === guard) extraUse = true;
        });
        if (extraUse) return false;
    } else {
        return false; // the guard must be j1's own fresh compare
    }
    const prefix: Inst[] = [];
    for (let i = 0; i < prefixEnd; i++) {
        const q = j1.insts[i]!;
        if (q.targets && q.targets.length > 0) return false;
        if (opInfo(q.op).effects !== Effect.NONE) return false;
        prefix.push(q);
    }

    // re-execution check: region2's guard failures re-run r1's slow chain
    // after r1's fast side ran (see checkShapeSlowReexec's argument)
    const fields = shapes.get(r1.shapeKey)!;
    if (!checkShapeSlowReexec(r1, fields, idom)) return false;

    // what the slow path knows each j1-defined value to be
    const slowMap = new Map<Inst, Inst>();
    const exitTarget = r1.slowExitEdge.inst.targets![r1.slowExitEdge.targetIndex]!;
    for (const p of j1.params) {
        const arg = exitTarget.args[j1.argIndexOfParam(p)];
        if (!arg) return false;
        slowMap.set(p, arg);
    }

    // routing pre-check (numeric merge verbatim): every use of a
    // j1-defined value outside region2 must be dominated by j2
    const routed: Inst[] = [...j1.params, ...prefix];
    const outsideUses = new Map<Inst, Inst[]>();
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
            if (blk === j1 || r2.fastBlocks.has(blk)) return;
            if (r2.slowSet.has(blk)) return; // substituted below
            if (!dominates(idom, j2, blk)) {
                ok = false;
                return;
            }
            outs.push(inst);
        });
        if (!ok) return false;
        if (outs.length > 0) {
            if (v.type !== "any") return false; // no raw-typed routing
            outsideUses.set(v, outs);
        }
    }

    // ---- all checks passed; mutate ----
    const mapSlow = (v: Inst): Inst => slowMap.get(v) ?? v;

    const slowExitBlock = r1.slowChain[r1.slowChain.length - 1]!;
    const exitInst = r1.slowExitEdge.inst;
    for (const q of prefix) {
        const clone = new Inst(fn, q.op, q.operands.map(mapSlow), { ...q.imms });
        clone.block = slowExitBlock;
        slowExitBlock.insts.splice(slowExitBlock.insts.indexOf(exitInst), 0, clone);
        slowMap.set(q, clone);
    }

    retargetEdge(exitInst, r1.slowExitEdge.targetIndex, r2.slowEntry, []);
    retargetEdge(r2.head.terminator!, 1, r1.slowEntry, []);
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

    stats.shape_regions_merged++;
    return true;
}

// P4.5: the heterogeneous merge — a NUMERIC guard region headed at a
// shape region's join merges into the shape region, exactly as a second
// shape region would: r2's has_tag failures reroute to r1's slow entry
// (r1's slow chain re-executes — checkShapeSlowReexec — then falls
// through into r2's slow chain, the full generic computation in program
// order).  After the merge r2's head params are fed only by r1's fast
// exits, so foldProvenGuards deletes the has_tag and rawJoinParams turns
// the join raw — the region computes unboxed end-to-end.  All checks
// precede all mutations; the check set is tryMergeAt's with r1's side
// verified by the mixed shape twin.
function tryMergeShapeNumericAt(
    fn: Func,
    shapes: Map<string, ShapeField[]>,
    r1: ShapeRegion,
    idom: Map<Block, Block>,
    stats: OptStats
): boolean {
    const j1 = r1.join;
    const r2 = matchRegionAt(j1);
    if (!r2) return false;
    const j2 = r2.join;

    // region2 strictly below region1 (no sharing, no cycles)
    if (j2 === r1.head || j2 === j1 || r1.fastBlocks.has(j2) || r1.slowSet.has(j2)) return false;
    if (r2.slowEntry === r1.slowEntry) return false;
    for (const b of r2.fastBlocks)
        if (r1.fastBlocks.has(b) || r1.slowSet.has(b) || b === r1.head) return false;
    for (const b of r2.slowChain)
        if (r1.fastBlocks.has(b) || r1.slowSet.has(b) || b === r1.head) return false;

    // j1's predecessors must be exactly region1's exits, j2's exactly
    // region2's (the numeric merge's review attack A)
    for (const e of j1.predEdges) {
        const src = e.inst.block!;
        if (!r1.fastBlocks.has(src) && !r1.slowSet.has(src)) return false;
    }
    for (const e of j2.predEdges) {
        const src = e.inst.block!;
        if (!r2.fastBlocks.has(src) && !r2.slowSet.has(src)) return false;
    }

    // both slow chains must be their fast sides' generic twins: r2 by the
    // numeric rule, r1 by the mixed shape rule
    if (!verifyGenericTwin(r2)) return false;
    if (!verifyShapeTwin(r1, shapes)) return false;

    // j1's instruction shape: [effect-free prefix..., (guard,) cond_br].
    // The numeric prefix logic verbatim — a has_tag guard is a fact about
    // an immutable SSA value, so unlike has_shape it need not be j1's own
    // fresh compare.
    const term = j1.terminator!;
    const guard = term.operands[0]!;
    let prefixEnd = j1.insts.length - 1;
    if (guard.block === j1) {
        if (j1.insts[j1.insts.length - 2] !== guard) return false;
        prefixEnd = j1.insts.length - 2;
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

    // re-execution check: r2's guard failures re-run r1's slow chain
    // after r1's fast side ran (see checkShapeSlowReexec's argument)
    const fields = shapes.get(r1.shapeKey);
    if (!fields) return false;
    if (!checkShapeSlowReexec(r1, fields, idom)) return false;

    // what the slow path knows each j1-defined value to be
    const slowMap = new Map<Inst, Inst>();
    const exitTarget = r1.slowExitEdge.inst.targets![r1.slowExitEdge.targetIndex]!;
    for (const p of j1.params) {
        const arg = exitTarget.args[j1.argIndexOfParam(p)];
        if (!arg) return false;
        slowMap.set(p, arg);
    }

    // routing pre-check (numeric merge verbatim): every use of a
    // j1-defined value outside region2 must be dominated by j2
    const routed: Inst[] = [...j1.params, ...prefix];
    const outsideUses = new Map<Inst, Inst[]>();
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
            if (blk === j1 || r2.fastBlocks.has(blk)) return;
            if (r2.slowSet.has(blk)) return; // substituted below
            if (!dominates(idom, j2, blk)) {
                ok = false;
                return;
            }
            outs.push(inst);
        });
        if (!ok) return false;
        if (outs.length > 0) {
            if (v.type !== "any") return false; // no raw-typed routing
            outsideUses.set(v, outs);
        }
    }

    // ---- all checks passed; mutate ----
    const mapSlow = (v: Inst): Inst => slowMap.get(v) ?? v;

    const slowExitBlock = r1.slowChain[r1.slowChain.length - 1]!;
    const exitInst = r1.slowExitEdge.inst;
    for (const q of prefix) {
        const clone = new Inst(fn, q.op, q.operands.map(mapSlow), { ...q.imms });
        clone.block = slowExitBlock;
        slowExitBlock.insts.splice(slowExitBlock.insts.indexOf(exitInst), 0, clone);
        slowMap.set(q, clone);
    }

    retargetEdge(exitInst, r1.slowExitEdge.targetIndex, r2.slowEntry, []);
    for (const ge of r2.guardFalseEdges) retargetEdge(ge.inst, ge.targetIndex, r1.slowEntry, []);
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

    stats.shape_numeric_merged++;
    return true;
}

// fold cond_brs on has_shape guards proven by an un-killed dominating
// shape fact (post-merge, region2's guard is exactly this)
function foldProvenShapeGuards(fn: Func, stats: OptStats): boolean {
    const analysis = computeShapeFacts(fn);
    if (!analysis) return false;
    let changed = false;
    for (const b of fn.blocks) {
        const term = b.terminator;
        if (!term || term.op !== "cond_br") continue;
        const cond = term.operands[0]!;
        if (cond.op !== "has_shape") continue;
        // the compare must be b's own: a fact at the BRANCH only proves a
        // FRESH compare true.  A has_shape computed in an earlier block can
        // be stale-false (the receiver transitioned into the shape after
        // it ran), and folding a stale-false branch to true would take the
        // wrong arm of arbitrary (attack) IR.  Same-block suffices: facts
        // never appear mid-block, so fact-at-branch implies fact-at-compare.
        if (cond.block !== b) continue;
        const facts = analysis.factsAt(b, b.insts.length - 1);
        if (!facts.has(shapeFactKey(cond.operands[0]!.id, String(cond.imms["shape"])))) continue;
        // facts were computed pre-fold; folding only removes edges, so the
        // stale analysis is conservative for the remaining candidates
        condBrToBr(fn, b, 0);
        stats.shape_guards_folded++;
        changed = true;
    }
    if (changed) sweepUnreachableBlocks(fn);
    return changed;
}

// run shape-region merging + fact folding to a fixpoint.  Cheap bail when
// the function has no shape guards (every flag-off compile).  Merging
// needs the module's shape table for the twin check; without one only
// folding runs (fail-closed).
export function optimizeShapeRegions(
    fn: Func,
    module: Module | undefined,
    stats: OptStats
): boolean {
    let hasGuard = false;
    for (const b of fn.blocks) {
        const t = b.terminator;
        if (t && t.op === "cond_br" && t.operands[0]!.op === "has_shape") {
            hasGuard = true;
            break;
        }
    }
    if (!hasGuard) return false;

    sweepUnreachableBlocks(fn);

    // P4.5 bisect hook (criterion 6): EJS_NO_SHAPE_FUSION disables the
    // heterogeneous merge + the in-loop numeric folding, leaving exactly
    // the P4.3 shape-region behavior (typed slot ACCESS is a contract
    // change and has no off switch — the verifier owns it).
    const noFusion = !!process.env["EJS_NO_SHAPE_FUSION"];
    let changedAny = false;
    for (let round = 0; round < 50; round++) {
        let changed = false;
        if (module) {
            for (let merges = 0; merges < 50; merges++) {
                const { rpo } = computeRPO(fn);
                const idom = computeDominators(fn, rpo);
                let merged = false;
                for (const b of rpo) {
                    const r1 = matchShapeRegionAt(b);
                    if (!r1) continue;
                    if (
                        tryMergeShapeAt(fn, module.shapes, r1, idom, stats) ||
                        (!noFusion && tryMergeShapeNumericAt(fn, module.shapes, r1, idom, stats))
                    ) {
                        merged = true;
                        changed = true;
                        break; // mutations invalidate matches; re-match
                    }
                }
                if (!merged) break;
            }
        }
        if (foldProvenShapeGuards(fn, stats)) changed = true;
        // P4.5: a heterogeneous merge leaves r2's has_tag guards fed only
        // by fast-side box_f64 values — provably numbers.  Folding them
        // here linearizes the fast side so the NEXT round's matcher can
        // grow the region further (the fusion cascade).
        if (!noFusion && foldProvenGuards(fn, stats)) changed = true;
        if (!changed) break;
        sweepUnreachableBlocks(fn);
        changedAny = true;
    }
    return changedAny;
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
