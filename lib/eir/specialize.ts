/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// typed calling convention / function specialization.  For a function with a LOCAL CLOSED WORLD — its
// closure value never escapes and every call site is enumerated
// in-module — emit a specialized clone with an unboxed signature
// (f64 formals, f64 result), rewrite the provably-known call sites to
// direct calls that unbox at the caller, and never emit the slow paths
// in the clone at all (SpecMode lowering).
//
// The trust story crosses the guarded line ON PURPOSE: oracle
// claims become facts inside the clone and at rewritten call sites.
// What keeps that honest:
//   - the differential harness (hard precondition) validates the
//     oracle's abstraction against concrete execution;
//   - the escape analysis here is COMPILER-side and structural (operand
//     flow over lowered EIR) — it does not consult the oracle, so a
//     wrong oracle can never widen the set of functions we specialize;
//     a function that LOOKS closed-world but isn't is rejected by
//     construction (any non-callee use of the closure value, or any
//     slot flow we can't fully enumerate, kills the candidate);
//   - structural post-checks on the lowered clone (env/this unused, no
//     frame ops, every return actually f64) discard any clone whose body
//     could not honor the signature — trust-free, independent of why.
//
// Two closure-flow shapes are recognized (v1):
//   - SSA-visible: every use of the make_closure value is the callee
//     of a plain call in the same function;
//   - promoted-slot: the single store of the closure into a promoted
//     (non-exported, module-private) "%self" slot, where every load of
//     that slot is used only as a plain-call callee.  Loads in the
//     storing function are rewritten when the store dominates the load;
//     loads elsewhere keep the generic path (still enumerated — they
//     call the generic function, never the clone).
//
// Module-level EXPORTS are never trusted-specialization candidates: the
// slot-based module ABI exposes boxed ejsvals to JS and native
// consumers (non-promoted slots are readable through importer slot
// loads and accessor functions), so only promoted slots — invisible
// outside the module — qualify.
//
// --- the escape taint and the export-boundary wrapper ---
//
// The analysis maam runs covers THIS module's executions only.  Any
// closure that escapes (canonically: stored in a non-promoted export
// slot) can be called by code the analysis never saw, and maam's value
// domain is constant-propagation — its claims about such a function's
// body may hold only for the argument CONSTANTS it analyzed, so even
// all-number external arguments can escape them.  Two consequences:
//
//   ESCAPE TAINT.  `tainted` is the set of Funcs whose activations can
//   observe un-analyzed values: the escaping closures themselves,
//   closed under (a) the callee of any enumerated call site hosted in
//   a tainted function (its arguments are tainted), and (b) any
//   closure created inside a tainted function (its captured
//   environment is tainted).  Unknown-callee calls need no edge: a
//   value only becomes callable from tainted code by flowing there,
//   which classifies its function as escaping.  Everything OUTSIDE the
//   set runs only during module init — before any external caller can
//   exist — so oracle claims about it keep their whole-program cover.
//   (Residual, documented: an import CYCLE can re-enter a module
//   mid-init; taint does not model that corner.)  No call site HOSTED
//   in a tainted function is ever rewritten to a trusted clone, and no
//   ESCAPING function gets one.  A tainted-but-non-escaping helper may
//   still be trusted-cloned: the clone is entered only through
//   rewritten sites in covered code, whose activations all run during
//   init — its tainted (generic-entry) activations never reach it.
//
//   THE WRAPPER.  An escaping entry point still gets a typed fast
//   path, but a trust-free one: an UNTRUSTED clone (guarded body —
//   ordinary diamonds, assume-and-guard gate, boxed "any" result) with
//   f64 formals boxed once at entry, plus a boundary wrapper spliced
//   into the generic function's entry: one has_tag(number) guard per
//   formal, all-pass dispatching to the clone via call_typed, any
//   failure falling through to the original generic body.  The entry
//   box_f64 proofs let the optimizer fold the formal-rooted diamonds
//   structurally, so the clone approaches trusted-clone quality
//   without consuming a single oracle claim as fact — exports keep
//   full dynamic semantics, external callers included.  Internal
//   callers reach the same guards through the generic entry (devirt
//   direct-calls it; LLVM can inline the prologue).
//   -fno-export-wrapper bisects the wrapper alone.

import { Module, Func, Inst, Block } from "./ir";
import { Effect, opInfo } from "./ops";
import { computeRPO, computeDominators, dominates } from "./verifier";
import { lowerSpecializedClone } from "./lower";
import type { ModCtx, SpecMode } from "./lower";
import type { ScopeAnalysis, FnInfo } from "./scopes";
import type { TypeOracle } from "./oracle";
import type * as e from "../estree";
import { passes } from "../pass-config";

export interface SpecStats {
    // clones emitted
    specialized: number;
    // call sites rewritten to call_typed
    sites: number;
    // candidates whose lowered clone failed the structural post-checks
    rejected: number;
    // escaping entry points that received a boundary wrapper
    wrapped: number;
    // call sites left generic because their host is escape-tainted
    fenced: number;
}

// one use of a value: the using instruction and where the value appears
interface Use {
    fn: Func;
    user: Inst;
    // operand index, or -1 when the value is a branch-edge argument
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

// does the oracle type this node as exactly {number}?  numeric literals
// qualify directly (the oracle's mapping policy leaves literals
// unmapped), mirroring LowerFunction.operandIsNumber.
function nodeIsNumber(oracle: TypeOracle, node: e.Node): boolean {
    const lit = node as { type?: string; value?: unknown };
    if (lit.type === "Literal") return typeof lit.value === "number";
    if (lit.type === "UnaryExpression") {
        const u = node as e.UnaryExpression;
        if (
            (u.operator === "-" || u.operator === "+") &&
            u.argument.type === "Literal" &&
            typeof (u.argument as e.Literal).value === "number"
        )
            return true;
    }
    const t = oracle.typeOfNode(node);
    return t.tags !== "top" && t.tags.size === 1 && t.tags.has("number");
}

// could this formal be a number at runtime?  The wrapper's oracle use is
// heuristic only (guards decide) — decline only a POSITIVE non-number
// claim, where the guard chain could never pass.
function nodeMayBeNumber(oracle: TypeOracle, node: e.Node): boolean {
    const t = oracle.typeOfNode(node);
    return t.tags === "top" || t.tags.has("number");
}

// the ReturnStatement nodes of fn's own body (nested functions excluded)
function ownReturns(fnNode: e.Function): e.ReturnStatement[] {
    const out: e.ReturnStatement[] = [];
    const walk = (n: unknown): void => {
        if (!n || typeof n !== "object") return;
        if (Array.isArray(n)) {
            for (const x of n) walk(x);
            return;
        }
        const node = n as { type?: string } & Record<string, unknown>;
        if (typeof node.type !== "string") return;
        if (
            node.type === "FunctionDeclaration" ||
            node.type === "FunctionExpression" ||
            node.type === "ArrowFunctionExpression"
        )
            return;
        if (node.type === "ReturnStatement") out.push(node as unknown as e.ReturnStatement);
        for (const k of Object.keys(node)) {
            if (k === "loc" || k === "range") continue;
            walk(node[k]);
        }
    };
    walk(fnNode.body);
    return out;
}

// ops a specialized clone must not contain (they need the generic
// calling convention's argc/args/newTarget/this machinery)
const CLONE_FRAME_OPS = new Set([
    "args_obj",
    "rest_args",
    "arg_len",
    "new_target",
    "construct_super",
    "construct_super_apply",
]);

interface CallSite {
    call: Inst;
    fn: Func;
    rewritable: boolean;
}

// the structural closure-flow classification of one function
interface ClosureFlow {
    // false when no make_closure for it remains (inlined/DCE'd)
    referenced: boolean;
    escapes: boolean;
    // complete only when !escapes (the scan stops at the first escape)
    sites: CallSite[];
}

// find the Func containing an instruction's block (blocks know their fn)
function fnOf(inst: Inst): Func {
    return inst.block!.fn;
}

function uniqueCloneName(m: Module, base: string): string {
    const names = new Set(m.functions.map((f) => f.name));
    let name = base;
    for (let i = 1; names.has(name); i++) name = `${base}$${i}`;
    return name;
}

// is `a` (in block ba at index ia) before `b` (in bb at ib) under dom?
function comesBefore(
    idom: Map<Block, Block>,
    a: Inst,
    b: Inst
): boolean {
    const ba = a.block!;
    const bb = b.block!;
    if (ba === bb) return ba.insts.indexOf(a) < bb.insts.indexOf(b);
    return dominates(idom, ba, bb);
}

// the escape-taint fixpoint (see the file comment).  Monotone across
// rounds: wrapper clones are added by the caller at minting time; the
// edges a wrapper clone contributes duplicate its generic twin's (same
// AST, same sites, same children), so membership never grows late.
function computeEscapeTaint(
    m: Module,
    flows: Map<FnInfo, ClosureFlow>,
    funcsByName: Map<string, Func>,
    tainted: Set<Func>
): void {
    for (const [info, flow] of flows) if (flow.escapes) tainted.add(info.fn!);
    for (;;) {
        let grew = false;
        // (a) a tainted host's call arguments are tainted values
        for (const [info, flow] of flows) {
            if (tainted.has(info.fn!)) continue;
            if (flow.sites.some((s) => tainted.has(s.fn))) {
                tainted.add(info.fn!);
                grew = true;
            }
        }
        // (b) a closure created in a tainted host captures tainted state
        for (const fn of m.functions) {
            if (!tainted.has(fn)) continue;
            fn.forEachInst((inst) => {
                if (inst.op !== "make_closure") return;
                const child = funcsByName.get(inst.imms["fn"] as string);
                if (child && !tainted.has(child)) {
                    tainted.add(child);
                    grew = true;
                }
            });
        }
        if (!grew) break;
    }
}

// splice the boundary wrapper into fn's entry: a fresh entry block takes
// over the calling-convention params, one has_tag(number) guard per
// formal chains toward the fast block (all-number: unbox, call_typed
// the clone, return its boxed result), and any guard failure branches
// to the original entry — the untouched generic body.
function installWrapper(fn: Func, spec: SpecMode): void {
    const oldEntry = fn.entry!;
    const formals = oldEntry.params.slice(2); // [0]=%env [1]=%this

    const newEntry = new Block(fn, "wrapentry");
    newEntry.params = oldEntry.params;
    for (const p of newEntry.params) p.block = newEntry;
    oldEntry.params = [];

    const guards: Block[] = [newEntry];
    for (let i = 1; i < formals.length; i++) guards.push(new Block(fn, "wrapguard"));
    const fast = new Block(fn, "wrapfast");
    for (const b of [...guards, fast]) b.sealed = true;

    for (let i = 0; i < formals.length; i++) {
        const b = guards[i]!;
        const t = new Inst(fn, "has_tag", [formals[i]!], { tag: "number" });
        const br = new Inst(fn, "cond_br", [t], {});
        for (const inst of [t, br]) {
            inst.block = b;
            b.insts.push(inst);
        }
        br.addTarget(i + 1 < formals.length ? guards[i + 1]! : fast, []);
        br.addTarget(oldEntry, []);
    }

    // the clone's ABI carries neither env nor `this` (post-checks
    // guarantee both unused); the EIR-level env operand is never emitted
    const envArg = new Inst(fn, "const", [], { kind: "undefined" });
    const unboxed = formals.map((f) => new Inst(fn, "unbox_f64", [f], {}));
    const call = new Inst(fn, "call_typed", [envArg, ...unboxed], { fn: spec.cloneName });
    const ret = new Inst(fn, "return", [call], {});
    for (const inst of [envArg, ...unboxed, call, ret]) {
        inst.block = fast;
        fast.insts.push(inst);
    }

    fn.blocks.splice(0, 0, ...guards, fast);
    fn.entry = newEntry;
}

export function specializeModule(
    m: Module,
    analysis: ScopeAnalysis,
    oracle: TypeOracle,
    this_module_info: { exports: Map<string, { slot_num: number; promoted?: boolean }> } | null,
    mod_ctx: ModCtx,
    stats: SpecStats
): boolean {
    // to a fixpoint: a freshly-lowered clone's body contains generic call
    // sites of OTHER specializable functions (sum$typed still calls
    // hypot2 through its slot) — each round re-enumerates over the module
    // as it now stands and rewrites what became visible.  `cloned`
    // remembers per-function outcomes (SpecMode = clone shipped, null =
    // clone rejected) so later rounds only add rewrites; `wrapped`
    // remembers wrapper judgments (installed or declined) — a wrapper
    // adds no rewritable sites, so one judgment is final.
    const cloned = new Map<FnInfo, SpecMode | null>();
    const wrapped = new Map<FnInfo, boolean>();
    // escape taint persists across rounds (wrapper clones join at
    // minting time); fencedSeen keeps the fence count per-site
    const tainted = new Set<Func>();
    const fencedSeen = new Set<Inst>();
    let changedAny = false;
    for (let round = 0; round < 5; round++) {
        if (
            !specializeRound(
                m,
                analysis,
                oracle,
                this_module_info,
                mod_ctx,
                stats,
                cloned,
                wrapped,
                tainted,
                fencedSeen
            )
        )
            break;
        changedAny = true;
    }
    return changedAny;
}

function specializeRound(
    m: Module,
    analysis: ScopeAnalysis,
    oracle: TypeOracle,
    this_module_info: { exports: Map<string, { slot_num: number; promoted?: boolean }> } | null,
    mod_ctx: ModCtx,
    stats: SpecStats,
    cloned: Map<FnInfo, SpecMode | null>,
    wrapped: Map<FnInfo, boolean>,
    tainted: Set<Func>,
    fencedSeen: Set<Inst>
): boolean {
    const uses = usesInModule(m);
    let toplevelFn: Func | null = null;
    for (const info of analysis.fnInfos.values())
        if (info.isToplevel && info.fn) toplevelFn = info.fn;

    const funcsByName = new Map<string, Func>();
    for (const fn of m.functions) funcsByName.set(fn.name, fn);

    // %self slot -> stores/loads, and slot -> promoted?
    const selfStores = new Map<number, Inst[]>();
    const selfLoads = new Map<number, Inst[]>();
    for (const fn of m.functions) {
        fn.forEachInst((inst) => {
            if (inst.imms["module"] !== "%self") return;
            const slot = inst.imms["slot"] as number;
            if (inst.op === "module_slot_store") {
                let l = selfStores.get(slot);
                if (!l) selfStores.set(slot, (l = []));
                l.push(inst);
            } else if (inst.op === "module_slot_load") {
                let l = selfLoads.get(slot);
                if (!l) selfLoads.set(slot, (l = []));
                l.push(inst);
            }
        });
    }
    const promotedSlots = new Set<number>();
    if (this_module_info)
        this_module_info.exports.forEach((info) => {
            if (info.promoted) promotedSlots.add(info.slot_num);
        });

    // per-function dominator trees, built lazily (only for functions that
    // actually host slot-load rewrites)
    const idoms = new Map<Func, Map<Block, Block>>();
    const idomOf = (fn: Func): Map<Block, Block> => {
        let d = idoms.get(fn);
        if (!d) {
            const { rpo } = computeRPO(fn);
            idoms.set(fn, (d = computeDominators(fn, rpo)));
        }
        return d;
    };

    // a plain closure-dispatch call using `v` as its callee?
    const calleeUse = (u: Use): boolean =>
        u.user.op === "call" && u.operandIndex === 0 && !u.user.imms["direct"];

    // one pass over the module: every make_closure, indexed by callee name
    const closuresByName = new Map<string, Inst[]>();
    for (const fn of m.functions) {
        fn.forEachInst((inst) => {
            if (inst.op !== "make_closure") return;
            const name = inst.imms["fn"] as string;
            let l = closuresByName.get(name);
            if (!l) closuresByName.set(name, (l = []));
            l.push(inst);
        });
    }

    // --- structural closure-flow (oracle-free), for every function -------
    // every flow of the closure value must end in a plain-call callee (or
    // the single store into a promoted module-private slot); anything
    // else is an escape.
    const closureFlowOf = (info: FnInfo): ClosureFlow => {
        const closures = closuresByName.get(info.name) || [];
        if (closures.length === 0) return { referenced: false, escapes: false, sites: [] };

        const sites: CallSite[] = [];
        for (const c of closures) {
            for (const u of uses.get(c) || []) {
                if (u.operandIndex === -1)
                    return { referenced: true, escapes: true, sites }; // edge arg
                if (calleeUse(u)) {
                    sites.push({
                        call: u.user,
                        fn: u.fn,
                        rewritable: !(u.user.targets && u.user.targets.length > 0),
                    });
                    continue;
                }
                // the one non-callee flow we can fully enumerate: the
                // single store into a promoted module-private slot
                if (
                    u.user.op === "module_slot_store" &&
                    u.user.imms["module"] === "%self" &&
                    u.operandIndex === 0
                ) {
                    const slot = u.user.imms["slot"] as number;
                    const stores = selfStores.get(slot) || [];
                    if (!promotedSlots.has(slot) || stores.length !== 1 || stores[0] !== u.user)
                        return { referenced: true, escapes: true, sites };
                    const store = u.user;
                    const storeFn = u.fn;
                    // a store in the toplevel ENTRY block with no
                    // CALL-effect instruction before it is
                    // cross-function-safe: no user code can run before
                    // the slot is initialized, so no load anywhere can
                    // observe the pre-store state — except a load
                    // TEXTUALLY earlier in the entry block itself, which
                    // reads the uninitialized slot (the documented
                    // hoisting-lost semantics) and must stay generic.
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
                    for (const load of selfLoads.get(slot) || []) {
                        for (const lu of uses.get(load) || []) {
                            if (lu.operandIndex === -1 || !calleeUse(lu))
                                return { referenced: true, escapes: true, sites };
                            // rewrite where the load provably yields this
                            // closure: after a prefix-safe store, any load
                            // except one earlier in the same entry block;
                            // otherwise same-function store-dominated only
                            // (elsewhere the generic slot path stands)
                            const loadFn = fnOf(load);
                            const orderOk = prefixSafe
                                ? !(
                                      load.block === store.block &&
                                      store.block!.insts.indexOf(load) <
                                          store.block!.insts.indexOf(store)
                                  )
                                : loadFn === storeFn &&
                                  comesBefore(idomOf(storeFn), store, load);
                            const rewritable =
                                orderOk && !(lu.user.targets && lu.user.targets.length > 0);
                            sites.push({ call: lu.user, fn: lu.fn, rewritable });
                        }
                    }
                    continue;
                }
                return { referenced: true, escapes: true, sites };
            }
        }
        return { referenced: true, escapes: false, sites };
    };

    const flows = new Map<FnInfo, ClosureFlow>();
    for (const info of analysis.fnInfos.values()) {
        if (info.isToplevel || !info.lowered || !info.fn) continue;
        flows.set(info, closureFlowOf(info));
    }

    computeEscapeTaint(m, flows, funcsByName, tainted);

    let changed = false;

    for (const [info, flow] of flows) {
        const node = info.node;

        // -fgen-eir generator bodies own a fixed resume-protocol ABI —
        // body(gen, mode, sent), every param boxed — and rewrite into
        // state machines after this pass; never clone or wrap them
        if (info.fn && info.fn.genBody) continue;

        // ===== escaping: the boundary-wrapper path =======================
        // (merely TAINTED functions — called from tainted hosts but not
        // escaping themselves — stay on the trusted path below: their
        // clones are entered only through rewritten sites in covered
        // code, and the fence keeps tainted-hosted sites generic.
        // Wrappers for tainted-called internal helpers / guarded
        // per-site dispatch are the recorded follow-on.)
        if (flow.escapes) {
            if (wrapped.has(info)) continue; // judged (installed or declined)
            if (!passes().exportWrapper) continue;
            if (!flow.referenced) continue;

            // static callee checks (AST side); >=1 formal or the guard
            // chain guards nothing
            if (info.restBinding || info.usesArguments) continue;
            if ((info.defaults || []).some((d) => d != null)) continue;
            if (!node.params.every((p) => p.type === "Identifier")) continue;
            if (node.params.length === 0) continue;
            // heuristic only (guards decide): skip formals the oracle
            // POSITIVELY types non-number — the chain could never pass
            if (!node.params.every((p) => nodeMayBeNumber(oracle, p))) continue;
            // the entry must own the calling convention outright
            if (info.fn!.entry!.predEdges.length > 0) continue;

            const spec: SpecMode = {
                cloneName: uniqueCloneName(m, `${info.name}$wrap`),
                trusted: false,
                formals: node.params.map(() => "f64" as const),
                result: "any",
            };
            const diamondsBefore = mod_ctx.typed_stats ? mod_ctx.typed_stats.diamonds : 0;
            const clone = lowerSpecializedClone(info, analysis, m, mod_ctx, spec);
            const diamondsAfter = mod_ctx.typed_stats ? mod_ctx.typed_stats.diamonds : 0;

            // structural post-checks: the unboxed ABI carries neither env
            // nor `this`, and no frame ops.  Returns stay boxed ("any"),
            // so no return check.  Payoff check: a clone that emitted no
            // diamonds has nothing for the entry proofs to fold.
            const cloneUses = new Map<Inst, number>();
            let ok = true;
            clone.forEachInst((inst) => {
                if (CLONE_FRAME_OPS.has(inst.op)) ok = false;
                for (const o of inst.operands) cloneUses.set(o, (cloneUses.get(o) || 0) + 1);
                if (inst.targets)
                    for (const t of inst.targets)
                        for (const a of t.args)
                            if (a) cloneUses.set(a, (cloneUses.get(a) || 0) + 1);
            });
            const envParam = clone.entry!.params[0]!;
            const thisParam = clone.entry!.params[1]!;
            if ((cloneUses.get(envParam) || 0) > 0) ok = false;
            if ((cloneUses.get(thisParam) || 0) > 0) ok = false;
            if (!ok || diamondsAfter - diamondsBefore === 0) {
                stats.rejected++;
                wrapped.set(info, false);
                continue;
            }
            m.addFunction(clone);
            tainted.add(clone); // its activations ARE the external entries
            installWrapper(info.fn!, spec);
            stats.wrapped++;
            wrapped.set(info, true);
            changed = true;
            continue;
        }

        // ===== covered (analysis-complete): the trusted path =============

        // a candidate this call already judged: null = clone was rejected
        // (don't re-lower it every round); a SpecMode = clone exists, only
        // NEW call sites (in later-lowered clone bodies) need rewriting
        const priorSpec = cloned.get(info);
        if (priorSpec === null) continue;

        if (priorSpec === undefined) {
            // --- static callee checks (AST side) -----------------------------
            if (info.restBinding || info.usesArguments) continue;
            if ((info.defaults || []).some((d) => d != null)) continue;
            if (!node.params.every((p) => p.type === "Identifier")) continue;

            // --- type profile: every formal and return exactly {number} ------
            if (!node.params.every((p) => nodeIsNumber(oracle, p))) continue;
            const returns = ownReturns(node);
            if (returns.length === 0) continue;
            if (!returns.every((r) => r.argument && nodeIsNumber(oracle, r.argument))) continue;
        }

        if (!flow.referenced || flow.escapes || flow.sites.length === 0) continue;
        const sites = flow.sites;

        // don't mint a clone no covered site will ever call (later rounds
        // re-judge: freshly-lowered covered clones can add sites)
        if (!priorSpec && !sites.some((s) => s.rewritable && !tainted.has(s.fn))) continue;

        let spec: SpecMode;
        if (priorSpec) {
            spec = priorSpec; // clone already shipped in an earlier round
        } else {
            // --- lower the clone (unguarded body, typed sig) ----------------
            spec = {
                cloneName: uniqueCloneName(m, `${info.name}$typed`),
                trusted: true,
                formals: node.params.map(() => "f64" as const),
                result: "f64",
            };
            const clone = lowerSpecializedClone(info, analysis, m, mod_ctx, spec);

            // --- structural post-checks (trust-free backstop) ---------------
            // the clone must actually honor the signature: env/this unused,
            // no frame ops, every return raw f64.  anything else discards it.
            const cloneUses = new Map<Inst, number>();
            let ok = true;
            clone.forEachInst((inst) => {
                if (CLONE_FRAME_OPS.has(inst.op)) ok = false;
                if (inst.op === "return" && inst.operands[0]!.type !== "f64") ok = false;
                for (const o of inst.operands) cloneUses.set(o, (cloneUses.get(o) || 0) + 1);
                if (inst.targets)
                    for (const t of inst.targets)
                        for (const a of t.args)
                            if (a) cloneUses.set(a, (cloneUses.get(a) || 0) + 1);
            });
            const envParam = clone.entry!.params[0]!;
            const thisParam = clone.entry!.params[1]!;
            // the entry box_f64 of each formal is the formal's ONLY allowed
            // use shape; env/this must be entirely unused
            if ((cloneUses.get(envParam) || 0) > 0) ok = false;
            if ((cloneUses.get(thisParam) || 0) > 0) ok = false;
            if (!ok) {
                stats.rejected++;
                cloned.set(info, null);
                continue;
            }
            m.addFunction(clone);
            cloned.set(info, spec);
            stats.specialized++;
            changed = true;
        }

        // --- rewrite the provably-known call sites --------------------------
        for (const site of sites) {
            if (!site.rewritable) continue;
            // the escape-taint fence: a site hosted in tainted code sees
            // values the analysis never covered — the generic call stands
            if (tainted.has(site.fn)) {
                if (!fencedSeen.has(site.call)) {
                    fencedSeen.add(site.call);
                    stats.fenced++;
                }
                continue;
            }
            const call = site.call;
            const g = site.fn;
            const args = call.operands.slice(2);
            if (args.length !== spec.formals.length) continue;
            const block = call.block!;
            const at = block.insts.indexOf(call);
            if (at < 0) continue;

            const insts: Inst[] = [];
            const envArg = new Inst(g, "const", [], { kind: "undefined" });
            insts.push(envArg);
            const unboxed = args.map((a) => {
                const u = new Inst(g, "unbox_f64", [a], {});
                insts.push(u);
                return u;
            });
            const direct = new Inst(g, "call_typed", [envArg, ...unboxed], {
                fn: spec.cloneName,
            });
            direct.type = "f64";
            insts.push(direct);
            const boxed = new Inst(g, "box_f64", [direct], {});
            insts.push(boxed);
            for (const i of insts) i.block = block;
            block.insts.splice(at, 0, ...insts);

            // point every consumer at the re-boxed result, then drop the
            // generic call (its callee/`this` operands lose their last use
            // and fall to DCE where possible)
            replaceCallWith(g, call, boxed);
            stats.sites++;
            changed = true;
        }
    }
    return changed;
}

function replaceCallWith(fn: Func, call: Inst, replacement: Inst): void {
    fn.forEachInst((inst) => {
        if (inst === replacement) return;
        for (let i = 0; i < inst.operands.length; i++)
            if (inst.operands[i] === call) inst.operands[i] = replacement;
        if (inst.targets)
            for (const t of inst.targets)
                for (let i = 0; i < t.args.length; i++)
                    if (t.args[i] === call) t.args[i] = replacement;
    });
    const b = call.block!;
    const idx = b.insts.indexOf(call);
    if (idx >= 0) b.insts.splice(idx, 1);
    call.block = null;
}
