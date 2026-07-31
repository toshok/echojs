/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// The "optimizer residue": the classic SSA cleanups and the
// type lattice.
//
//   (a) a trust-free TYPE LATTICE over the boxed `any` values —
//       value-intrinsic tags (const kinds, allocation ops, generic ops
//       with fixed result types) met over block-param edges to a
//       fixpoint.  Nothing here consumes an oracle claim; every tag is
//       proven from the IR, so the lattice is sound on flag-off
//       compiles too.
//   (b) CONSTANT FOLDING over primitive consts, evaluated in the
//       hosting engine (both host and target implement the same ES
//       semantics for primitive arithmetic/comparison; folds that
//       would mint a STRING from a non-string — number formatting —
//       are excluded, as are string relational compares, so a host/
//       runtime divergence in either can never be baked in at compile
//       time.  typeof folds follow the RUNTIME's mapping, including
//       its `typeof null == "null"` quirk).
//   (c) REDUNDANT to_boolean/typeof ELIMINATION: cond_br on a
//       known-truthiness to_boolean folds; `to_boolean(logical_not x)`
//       inverts the branch instead of calling _ejs_op_not + _ejs_truthy;
//       `typeof x === "T"` becomes the single-tag-test typeof_is op.
//   (d) TRIVIAL BLOCK PARAM pruning (the SSA form of copy
//       propagation): a param fed the same SSA value on every edge is
//       that value (the value dominates every pred, hence the block,
//       hence every use of the param).
//   (e) LATTICE-TYPED LOW-TIER LOWERING — the "feeding the low-tier
//       ops beyond what the oracle already types" item: a generic
//       add/sub/mul/div both of whose operands the lattice proves
//       number computes bit-identically in f64 (ES semantics; the
//       guard-region soundness inventory's argument), so it lowers to
//       unbox/f64_*/box with no guard at all.  lt/gt lower to f64_lt
//       when their only consumer is a same-block to_boolean + cond_br.
//   (f) MODULE-SLOT LOAD CSE (the toplevel-receiver reload):
//         - block-local availability, killed at CALL-effect
//           instructions (arbitrary JS may re-enter this module's
//           stores) unless the slot is single-store (below), with
//           store-to-load forwarding;
//         - single-store %self slots: the module init flag is set
//           BEFORE the toplevel body runs (compiler.ts
//           emitModuleResolution), so the toplevel executes at most
//           once per process and a %self slot whose ONLY static store
//           sits in the toplevel entry block is immutable once
//           written.  In the toplevel itself, every load the store
//           comes-before folds to the stored value; in any other
//           function the slot cannot change during an activation (the
//           suspended init's remaining stores can only run after this
//           function returns; re-entry is blocked by the flag), so a
//           dominated load folds to its dominator.  The census counts
//           EVERY module_slot_store — including export-accessor
//           setters — so an externally-writable binding never
//           qualifies.
//
// Pass placement (optimize.ts): CSE runs before the guard/shape region
// passes (receiver identity is what lets toplevel regions merge); the
// folding passes run AFTER them — like foldUnboxOfBox, folding
// arithmetic earlier would perturb the exact IR shapes the region
// matchers verify.

import { Func, Block, Inst, replaceAllUses } from "./ir";
import type { Target } from "./ir";
import { Effect, opInfo } from "./ops";
import { condBrToBr, sweepUnreachableBlocks } from "./optimize-guards";
import { computeRPO, computeDominators, dominates } from "./verifier";
import type { OptStats } from "./optimize";

// --- the type lattice -------------------------------------------------------

// flat lattice over the boxed value tags: undefined (in the array) is
// bottom (no information yet), "top" is no-information-possible.  The
// tags mirror the runtime's tag taxonomy (typeof-null quirk included:
// null is its own tag here AND in _ejs_op_typeof).
export type LatticeTag =
    | "number"
    | "string"
    | "boolean"
    | "undefined"
    | "null"
    | "object"
    | "function"
    | "top";

export type Lattice = (LatticeTag | undefined)[];

// generic ops whose result is always a Number (ES: they apply
// ToNumber/ToInt32/ToUint32 and produce a Number or throw;
// runtime/ejs-ops.c agrees — only NUMBER_TO_EJSVAL returns)
const NUMBER_RESULT = new Set([
    "sub",
    "mul",
    "div",
    "mod",
    "neg",
    "unary_plus",
    "bitand",
    "bitor",
    "bitxor",
    "shl",
    "shr",
    "ushr",
    "bitnot",
]);

// generic ops whose result is always a Boolean
const BOOLEAN_RESULT = new Set([
    "lt",
    "le",
    "gt",
    "ge",
    "loose_eq",
    "loose_neq",
    "strict_eq",
    "strict_neq",
    "logical_not",
    "instanceof",
    "in",
    "typeof_is",
]);

// ops that always produce a (non-callable) Object
const OBJECT_RESULT = new Set([
    "make_object",
    "make_object_shaped",
    "make_array",
    "make_regexp",
    "args_obj",
    "rest_args",
    "array_from_spread",
    "template_callsite",
]);

function meet(a: LatticeTag | undefined, b: LatticeTag | undefined): LatticeTag | undefined {
    if (a === undefined) return b;
    if (b === undefined) return a;
    return a === b ? a : "top";
}

function constTag(inst: Inst): LatticeTag {
    switch (inst.imms["kind"] as string) {
        case "number":
            return "number";
        case "atom":
            return "string";
        case "boolean":
            return "boolean";
        case "undefined":
            return "undefined";
        case "null":
            return "null";
        default:
            return "top";
    }
}

// one evaluation of the transfer function for `inst` under `tags`
function instTag(inst: Inst, tags: Lattice): LatticeTag | undefined {
    const op = inst.op;
    if (op === "const") return constTag(inst);
    if (op === "box_f64") return "number";
    if (NUMBER_RESULT.has(op)) return "number";
    if (BOOLEAN_RESULT.has(op)) return "boolean";
    if (OBJECT_RESULT.has(op)) return "object";
    if (op === "make_closure") return "function";
    if (op === "typeof") return "string";
    if (op === "add") {
        // string if either side is a string (ES: a string primitive on
        // either side means concatenation); number only if both sides
        // are numbers; anything else can go either way (objects'
        // ToPrimitive decides at runtime)
        const a = tags[inst.operands[0]!.id];
        const b = tags[inst.operands[1]!.id];
        if (a === "string" || b === "string") return "string";
        if (a === undefined || b === undefined) return undefined;
        if (a === "number" && b === "number") return "number";
        return "top";
    }
    if (op === "blockparam") {
        const b = inst.block!;
        // entry params are the calling convention's values; exception
        // params carry whatever was thrown
        if (b === b.fn.entry || inst.isException || b.isCatch) return "top";
        if (b.predEdges.length === 0) return undefined; // unreachable
        const argIdx = b.argIndexOfParam(inst);
        let t: LatticeTag | undefined = undefined;
        for (const e of b.predEdges) {
            const arg = e.inst.targets![e.targetIndex]!.args[argIdx];
            if (!arg) return "top";
            if (arg === inst) continue; // self-edge: vacuous
            t = meet(t, tags[arg.id]);
            if (t === "top") return t;
        }
        return t;
    }
    return "top";
}

// fixpoint over the whole function.  The lattice has height 2, so the
// loop terminates quickly; the bound is belt and braces.
export function computeLattice(fn: Func): Lattice {
    const tags: Lattice = new Array(fn.next_value_id);
    for (let round = 0; round < 20; round++) {
        let changed = false;
        fn.forEachInst((inst) => {
            const t = instTag(inst, tags);
            if (t !== undefined && tags[inst.id] !== t) {
                // monotone by construction (undefined -> tag -> top)
                tags[inst.id] = t;
                changed = true;
            }
        });
        if (!changed) break;
    }
    return tags;
}

// --- constant folding -------------------------------------------------------

// the JS payload of a primitive const
function constPayload(inst: Inst): unknown {
    switch (inst.imms["kind"] as string) {
        case "undefined":
            return undefined;
        case "null":
            return null;
        default:
            return inst.imms["value"];
    }
}

// rewrite `inst` in place into a primitive const (same Inst object
// keeps every use — the foldUnboxOfBox precedent)
function toConst(inst: Inst, value: unknown, stats: OptStats): void {
    let imms: Inst["imms"];
    if (value === undefined) imms = { kind: "undefined" };
    else if (value === null) imms = { kind: "null" };
    else if (typeof value === "number") imms = { kind: "number", value: value };
    else if (typeof value === "boolean") imms = { kind: "boolean", value: value };
    else imms = { kind: "atom", value: String(value) };
    inst.op = "const";
    inst.operands.length = 0;
    inst.imms = imms;
    inst.type = "any";
    stats.consts_folded++;
}

// binops foldable by evaluating the SAME ES semantics in the hosting
// engine.  Relational ops are restricted to number operands (string
// relational compare is the one place a host/runtime collation
// difference could hide); results are accepted only when they are
// numbers/booleans, or strings made purely from strings.
const EVAL_BINOPS = new Set([
    "add",
    "sub",
    "mul",
    "div",
    "mod",
    "bitand",
    "bitor",
    "bitxor",
    "shl",
    "shr",
    "ushr",
    "lt",
    "le",
    "gt",
    "ge",
    "loose_eq",
    "loose_neq",
    "strict_eq",
    "strict_neq",
]);

const RELATIONAL = new Set(["lt", "le", "gt", "ge"]);

/* eslint-disable @typescript-eslint/no-explicit-any */
function evalBinop(op: string, x: any, y: any): unknown {
    switch (op) {
        case "add":
            return x + y;
        case "sub":
            return x - y;
        case "mul":
            return x * y;
        case "div":
            return x / y;
        case "mod":
            return x % y;
        case "bitand":
            return x & y;
        case "bitor":
            return x | y;
        case "bitxor":
            return x ^ y;
        case "shl":
            return x << y;
        case "shr":
            return x >> y;
        case "ushr":
            return x >>> y;
        case "lt":
            return x < y;
        case "le":
            return x <= y;
        case "gt":
            return x > y;
        case "ge":
            return x >= y;
        case "loose_eq":
            return x == y;
        case "loose_neq":
            return x != y;
        case "strict_eq":
            return x === y;
        case "strict_neq":
            return x !== y;
        default:
            return undefined;
    }
}

function evalUnop(op: string, x: any): unknown {
    switch (op) {
        case "neg":
            return -x;
        case "unary_plus":
            return +x;
        case "bitnot":
            return ~x;
        case "logical_not":
            return !x;
        default:
            return undefined;
    }
}
/* eslint-enable @typescript-eslint/no-explicit-any */

// the runtime's typeof string for a lattice tag (_ejs_op_typeof —
// spec mapping, typeof null is "object")
const TYPEOF_OF_TAG: Record<string, string | undefined> = {
    number: "number",
    string: "string",
    boolean: "boolean",
    undefined: "undefined",
    null: "object",
    object: "object",
    function: "function",
};

function foldConstants(fn: Func, tags: Lattice, stats: OptStats): boolean {
    let changed = false;
    fn.forEachInst((inst) => {
        if (inst.targets && inst.targets.length > 0) return; // protected-region terminator
        const op = inst.op;
        if (op === "typeof") {
            const t = tags[inst.operands[0]!.id];
            const s = t && t !== "top" ? TYPEOF_OF_TAG[t] : undefined;
            if (s !== undefined) {
                toConst(inst, s, stats);
                changed = true;
            }
            return;
        }
        if (EVAL_BINOPS.has(op)) {
            const a = inst.operands[0]!;
            const b = inst.operands[1]!;
            if (a.op !== "const" || b.op !== "const") return;
            if (
                RELATIONAL.has(op) &&
                (a.imms["kind"] !== "number" || b.imms["kind"] !== "number")
            )
                return;
            const r = evalBinop(op, constPayload(a), constPayload(b));
            if (typeof r === "number" || typeof r === "boolean") {
                toConst(inst, r, stats);
                changed = true;
            } else if (
                typeof r === "string" &&
                a.imms["kind"] === "atom" &&
                b.imms["kind"] === "atom"
            ) {
                toConst(inst, r, stats);
                changed = true;
            }
            return;
        }
        if (op === "neg" || op === "unary_plus" || op === "bitnot" || op === "logical_not") {
            const a = inst.operands[0]!;
            if (a.op !== "const") return;
            if (op !== "logical_not" && a.imms["kind"] !== "number") return;
            const r = evalUnop(op, constPayload(a));
            if (typeof r === "number" || typeof r === "boolean") {
                toConst(inst, r, stats);
                changed = true;
            }
        }
    });
    return changed;
}

// --- typeof_is peephole -----------------------------------------------------

// `typeof x === "T"` (either operand order) is a single runtime tag
// test.  The rewrite is exact per _ejs_op_typeof's mapping (the
// runtime's typeof_is_<T> tests the same predicate typeof compares
// against — typeof_is_object admits null, typeof_is_null is constant
// false); the typeof goes dead and DCE sweeps it.  Only the types with
// runtime.ts entries qualify.
const TYPEOF_IS_TYPES = new Set([
    "object",
    "function",
    "string",
    "number",
    "undefined",
    "null",
    "boolean",
]);

function typeofIsPeephole(fn: Func, stats: OptStats): boolean {
    let changed = false;
    fn.forEachInst((inst) => {
        if (inst.op !== "strict_eq") return;
        let tof = inst.operands[0]!;
        let lit = inst.operands[1]!;
        if (tof.op !== "typeof") {
            const t = tof;
            tof = lit;
            lit = t;
        }
        if (tof.op !== "typeof" || tof.targets) return;
        if (lit.op !== "const" || lit.imms["kind"] !== "atom") return;
        const ty = lit.imms["value"] as string;
        if (!TYPEOF_IS_TYPES.has(ty)) return;
        inst.op = "typeof_is";
        inst.operands.length = 0;
        inst.operands.push(tof.operands[0]!);
        inst.imms = { type: ty };
        stats.typeof_rewrites++;
        changed = true;
    });
    return changed;
}

// --- branch folding + logical_not inversion ---------------------------------

// truthiness of a value, when provable: consts decide exactly;
// undefined/null are always falsy; objects and functions are always
// truthy (no document.all in this runtime).
function knownTruthiness(v: Inst, tags: Lattice): boolean | undefined {
    if (v.op === "const") return !!constPayload(v);
    const t = tags[v.id];
    if (t === "undefined" || t === "null") return false;
    if (t === "object" || t === "function") return true;
    return undefined;
}

// tags that can never carry a shape header.  NB: there is deliberately
// NO has_tag FALSE-folding here — a boxed-repr slot_store's verifier
// proof IS a dominating has_tag=false fact, and folding the branch
// deletes the fact out from under the surviving store (caught by the
// --types lane on every class file).  has_shape folds are safe: the
// slot ops that need the fact live in the folded-away fast arm.
const NEVER_SHAPED = new Set(["number", "string", "boolean", "undefined", "null"]);

function foldBranches(fn: Func, tags: Lattice, stats: OptStats): boolean {
    let changed = false;
    // to_boolean use counts, for the inversion's locality check
    const uses = new Map<Inst, number>();
    fn.forEachInst((inst) => {
        for (const o of inst.operands) if (o.op === "to_boolean" || o.op === "logical_not")
            uses.set(o, (uses.get(o) || 0) + 1);
        if (inst.targets)
            for (const t of inst.targets)
                for (const a of t.args)
                    if (a && (a.op === "to_boolean" || a.op === "logical_not"))
                        uses.set(a, (uses.get(a) || 0) + 1);
    });

    for (const b of fn.blocks) {
        const term = b.terminator;
        if (!term || term.op !== "cond_br") continue;
        const cond = term.operands[0]!;
        if (cond.op === "to_boolean") {
            // invert through logical_not first: branching on !x is
            // branching on x with the targets swapped.  Sound only when
            // this cond_br is the to_boolean's single consumer (the
            // rewrite changes its meaning).
            let inverted = true;
            while (inverted) {
                inverted = false;
                const src = cond.operands[0]!;
                if (
                    src.op === "logical_not" &&
                    !src.targets &&
                    uses.get(cond) === 1 &&
                    cond.block === b
                ) {
                    cond.operands[0] = src.operands[0]!;
                    const t0: Target = term.targets![0]!;
                    const t1: Target = term.targets![1]!;
                    term.targets![0] = t1;
                    term.targets![1] = t0;
                    // predEdges' targetIndex must track the swap (both
                    // targets may name the same block — flip each edge
                    // exactly once)
                    const targetBlocks = new Set<Block>([t0.block, t1.block]);
                    for (const blk of targetBlocks) {
                        for (const e of blk.predEdges) {
                            if (e.inst === term) e.targetIndex = e.targetIndex === 0 ? 1 : 0;
                        }
                    }
                    uses.set(src, (uses.get(src) || 1) - 1);
                    stats.branches_folded++;
                    changed = true;
                    inverted = true;
                }
            }
            const truth = knownTruthiness(cond.operands[0]!, tags);
            if (truth !== undefined) {
                condBrToBr(fn, b, truth ? 0 : 1);
                stats.branches_folded++;
                changed = true;
            }
        } else if (cond.op === "has_shape") {
            const t = tags[cond.operands[0]!.id];
            if (t && NEVER_SHAPED.has(t)) {
                condBrToBr(fn, b, 1);
                stats.branches_folded++;
                changed = true;
            }
        }
    }
    return changed;
}

// --- trivial block params ---------------------------------------------------

// a param fed the same SSA value on every edge (self-edges vacuous) IS
// that value: the value's def dominates every pred's terminator, hence
// the param's block, hence every use of the param.
function pruneTrivialParams(fn: Func, stats: OptStats): boolean {
    let changed = false;
    for (const b of fn.blocks) {
        if (b === fn.entry) continue; // calling convention
        for (const p of b.params.slice()) {
            if (p.removed || p.isException || p.type !== "any" || p.rawJoin) continue;
            if (b.predEdges.length === 0) continue;
            const argIdx = b.argIndexOfParam(p);
            let v: Inst | null = null;
            let ok = true;
            for (const e of b.predEdges) {
                const arg = e.inst.targets![e.targetIndex]!.args[argIdx];
                if (!arg) {
                    ok = false;
                    break;
                }
                if (arg === p) continue;
                if (v === null) v = arg;
                else if (v !== arg) {
                    ok = false;
                    break;
                }
            }
            if (!ok || v === null) continue;
            replaceAllUses(fn, p, v);
            b.removeParam(p);
            stats.params_pruned++;
            changed = true;
        }
    }
    return changed;
}

// --- lattice-typed low-tier lowering ----------------------------------------

const F64_OP: Record<string, string | undefined> = {
    add: "f64_add",
    sub: "f64_sub",
    mul: "f64_mul",
    div: "f64_div",
};

function insertBefore(fn: Func, anchor: Inst, inst: Inst): Inst {
    const b = anchor.block!;
    inst.block = b;
    b.insts.splice(b.insts.indexOf(anchor), 0, inst);
    return inst;
}

function removeFromBlock(inst: Inst): void {
    const b = inst.block!;
    const idx = b.insts.indexOf(inst);
    if (idx >= 0) b.insts.splice(idx, 1);
    inst.block = null;
}

function latticeLowerArith(fn: Func, tags: Lattice, stats: OptStats): boolean {
    let changed = false;

    // use map for the lt/gt consumer-pattern check
    const uses = new Map<Inst, Inst[]>();
    fn.forEachInst((inst) => {
        for (const o of inst.operands) {
            let l = uses.get(o);
            if (!l) uses.set(o, (l = []));
            l.push(inst);
        }
        if (inst.targets)
            for (const t of inst.targets)
                for (const a of t.args)
                    if (a) {
                        let l = uses.get(a);
                        if (!l) uses.set(a, (l = []));
                        l.push(inst);
                    }
    });

    const bothNumber = (inst: Inst) =>
        tags[inst.operands[0]!.id] === "number" && tags[inst.operands[1]!.id] === "number";

    const worklist: Inst[] = [];
    fn.forEachInst((inst) => worklist.push(inst));

    for (const inst of worklist) {
        if (!inst.block) continue;
        if (inst.targets && inst.targets.length > 0) continue;
        const op = inst.op;

        if (F64_OP[op] !== undefined && bothNumber(inst)) {
            const a = inst.operands[0]!;
            const b = inst.operands[1]!;
            if (a.op === "const" && b.op === "const") continue; // constFold's job
            const ua = insertBefore(fn, inst, new Inst(fn, "unbox_f64", [a], {}));
            const ub = insertBefore(fn, inst, new Inst(fn, "unbox_f64", [b], {}));
            const f = insertBefore(fn, inst, new Inst(fn, F64_OP[op]!, [ua, ub], {}));
            const boxed = insertBefore(fn, inst, new Inst(fn, "box_f64", [f], {}));
            replaceAllUses(fn, inst, boxed);
            removeFromBlock(inst);
            stats.lattice_arith++;
            changed = true;
            continue;
        }

        if (op === "unary_plus" && tags[inst.operands[0]!.id] === "number") {
            // +x for a number x is x
            replaceAllUses(fn, inst, inst.operands[0]!);
            removeFromBlock(inst);
            stats.lattice_arith++;
            changed = true;
            continue;
        }

        if ((op === "lt" || op === "gt") && bothNumber(inst)) {
            // only the whole same-block lt/to_boolean/cond_br chain
            // rewrites: f64_lt's i1 must not leak anywhere else
            const us = uses.get(inst) || [];
            if (us.length !== 1) continue;
            const tob = us[0]!;
            if (tob.op !== "to_boolean" || tob.block !== inst.block) continue;
            const tobUses = uses.get(tob) || [];
            if (tobUses.length !== 1) continue;
            const cbr = tobUses[0]!;
            if (cbr.op !== "cond_br" || cbr.block !== inst.block) continue;
            const a = inst.operands[0]!;
            const b = inst.operands[1]!;
            const ua = insertBefore(fn, inst, new Inst(fn, "unbox_f64", [a], {}));
            const ub = insertBefore(fn, inst, new Inst(fn, "unbox_f64", [b], {}));
            // a > b is b < a for numbers (NaN compares false either way)
            const f =
                op === "lt"
                    ? new Inst(fn, "f64_lt", [ua, ub], {})
                    : new Inst(fn, "f64_lt", [ub, ua], {});
            insertBefore(fn, inst, f);
            cbr.operands[0] = f;
            removeFromBlock(tob);
            removeFromBlock(inst);
            stats.lattice_arith++;
            changed = true;
        }
    }
    return changed;
}

// --- driver -----------------------------------------------------------------

export function cleanupFunction(fn: Func, stats: OptStats): boolean {
    let any = false;
    for (let round = 0; round < 5; round++) {
        const tags = computeLattice(fn);
        let changed = false;
        if (foldConstants(fn, tags, stats)) changed = true;
        if (typeofIsPeephole(fn, stats)) changed = true;
        if (foldBranches(fn, tags, stats)) {
            sweepUnreachableBlocks(fn);
            changed = true;
        }
        if (pruneTrivialParams(fn, stats)) changed = true;
        if (latticeLowerArith(fn, tags, stats)) changed = true;
        if (!changed) break;
        any = true;
    }
    return any;
}

// --- module-slot load CSE ---------------------------------------------------

export function slotKey(module: string, slot: number): string {
    return `${module}#${slot}`;
}

// the STABLE %self slots: exactly one module_slot_store in the whole
// module, sitting in the toplevel's entry block (which has no
// back-edges and — via the init flag set before the body runs — can
// execute at most once per process).  A stable slot's value cannot
// change during any function activation: the toplevel's remaining
// stores only resume after a callee returns, and re-entry is blocked
// by the flag.  Export-accessor setters are module_slot_stores too, so
// an externally-writable export can never look stable.
export function computeStableSlots(fns: readonly Func[], toplevelName: string): Set<string> {
    const count = new Map<string, number>();
    const storeIn = new Map<string, { fn: Func; inst: Inst }>();
    for (const fn of fns) {
        fn.forEachInst((inst) => {
            if (inst.op !== "module_slot_store") return;
            const key = slotKey(inst.imms["module"] as string, inst.imms["slot"] as number);
            count.set(key, (count.get(key) || 0) + 1);
            storeIn.set(key, { fn, inst });
        });
    }
    const stable = new Set<string>();
    count.forEach((n, key) => {
        if (n !== 1 || !key.startsWith("%self#")) return;
        const s = storeIn.get(key)!;
        if (s.fn.name !== toplevelName) return;
        const entry = s.fn.entry;
        if (!entry || s.inst.block !== entry || entry.predEdges.length > 0) return;
        stable.add(key);
    });
    return stable;
}

// is `a` before `b`: same block by position, else by dominance
function comesBefore(idom: Map<Block, Block>, a: Inst, b: Inst): boolean {
    const ba = a.block!;
    const bb = b.block!;
    if (ba === bb) return ba.insts.indexOf(a) < bb.insts.indexOf(b);
    return dominates(idom, ba, bb);
}

export function cseModuleSlotLoads(
    fn: Func,
    stableSlots: Set<string> | undefined,
    stats: OptStats
): boolean {
    let changed = false;

    // the stability arguments below reason about one ACTIVATION: a
    // suspendable activation (a desugared generator body — its yields
    // lower to generator_* runtime calls) can see the toplevel's
    // remaining stores run mid-flight, so it gets no exemptions.
    let suspends = false;
    fn.forEachInst((inst) => {
        if (
            inst.op === "call_runtime" &&
            typeof inst.imms["name"] === "string" &&
            (inst.imms["name"] as string).indexOf("generator_") === 0
        )
            suspends = true;
    });

    const isStable = (key: string) => !suspends && !!stableSlots && stableSlots.has(key);

    // (1) block-local availability + store-to-load forwarding
    for (const b of fn.blocks) {
        const avail = new Map<string, Inst>();
        for (const inst of b.insts.slice()) {
            if (inst.block !== b) continue; // removed below
            if (inst.op === "module_slot_load") {
                const key = slotKey(inst.imms["module"] as string, inst.imms["slot"] as number);
                const prev = avail.get(key);
                if (prev) {
                    replaceAllUses(fn, inst, prev);
                    removeFromBlock(inst);
                    stats.slot_loads_cse++;
                    changed = true;
                } else {
                    avail.set(key, inst);
                }
            } else if (inst.op === "module_slot_store") {
                const key = slotKey(inst.imms["module"] as string, inst.imms["slot"] as number);
                avail.set(key, inst.operands[0]!);
            } else if ((opInfo(inst.op).effects & Effect.CALL) !== 0) {
                // arbitrary JS may execute this module's stores — only
                // stable slots survive (their one store cannot run
                // mid-activation; see computeStableSlots)
                for (const key of Array.from(avail.keys())) {
                    if (!isStable(key)) avail.delete(key);
                }
            }
        }
    }

    // (2) the stable-slot dominance tier
    if (!suspends && stableSlots && stableSlots.size > 0) {
        const loadsByKey = new Map<string, Inst[]>();
        const storeByKey = new Map<string, Inst>();
        fn.forEachInst((inst) => {
            if (inst.op === "module_slot_load") {
                const key = slotKey(inst.imms["module"] as string, inst.imms["slot"] as number);
                let l = loadsByKey.get(key);
                if (!l) loadsByKey.set(key, (l = []));
                l.push(inst);
            } else if (inst.op === "module_slot_store") {
                const key = slotKey(inst.imms["module"] as string, inst.imms["slot"] as number);
                storeByKey.set(key, inst);
            }
        });

        let idom: Map<Block, Block> | null = null;
        let blockOrder: Map<Block, number> | null = null;
        const domInfo = () => {
            if (!idom) {
                const { rpo } = computeRPO(fn);
                idom = computeDominators(fn, rpo);
                blockOrder = new Map();
                rpo.forEach((b, i) => blockOrder!.set(b, i));
            }
            return { idom: idom!, blockOrder: blockOrder! };
        };

        loadsByKey.forEach((loads, key) => {
            if (!isStable(key)) return;
            const store = storeByKey.get(key);
            if (store && store.block) {
                // the module's one store lives in THIS function (so
                // this IS the toplevel, per computeStableSlots): loads
                // the store comes-before fold to the stored value
                const { idom } = domInfo();
                const value = store.operands[0]!;
                for (const load of loads) {
                    if (!load.block) continue; // CSE'd by the block-local tier
                    if (!comesBefore(idom, store, load)) continue; // pre-init read
                    replaceAllUses(fn, load, value);
                    removeFromBlock(load);
                    stats.slot_loads_cse++;
                    changed = true;
                }
            } else {
                // store elsewhere (the toplevel init): the slot cannot
                // change during this activation — dominated loads fold
                // to their dominators
                const { idom, blockOrder } = domInfo();
                const live = loads.filter((l) => l.block !== null);
                live.sort((a, b) => {
                    const ba = blockOrder.get(a.block!) ?? 0;
                    const bb = blockOrder.get(b.block!) ?? 0;
                    if (ba !== bb) return ba - bb;
                    return a.block!.insts.indexOf(a) - b.block!.insts.indexOf(b);
                });
                const survivors: Inst[] = [];
                for (const load of live) {
                    let folded = false;
                    for (const p of survivors) {
                        if (comesBefore(idom, p, load)) {
                            replaceAllUses(fn, load, p);
                            removeFromBlock(load);
                            stats.slot_loads_cse++;
                            changed = true;
                            folded = true;
                            break;
                        }
                    }
                    if (!folded) survivors.push(load);
                }
            }
        });
    }

    return changed;
}
