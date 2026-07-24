/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// EIR structural verifier.  checks:
//   - every opcode exists and respects its arity
//   - every block is sealed and ends in exactly one terminator
//   - branch edge argument counts match the target's parameter counts
//   - every operand's definition dominates its use (standard iterative
//     dominance computation over the CFG)
//
// verify() throws on the first violation; the error message names the
// function, block, and instruction involved.

import { opInfo, isTerminator, Effect } from "./ops";
import { printInst } from "./printer";
import type { Func, Block, Inst, Module } from "./ir";

// --- shape guard facts (shapes-plan P4.3) -----------------------------------
//
// The effect-kill soundness inventory for shape facts, in one place (this
// is THE new hazard class this phase adds — see docs/shapes-plan.md):
//
//   - A fact "(value v, shape S)" means: on every path to here, a
//     has_shape(v, S) compare executed, answered true, and NO instruction
//     that can change any object's shape has run since.  Under that fact a
//     slot_load/slot_store on v at S-derived indices is safe: the guard
//     proved v is an ordinary shaped object whose storage word is a slot
//     array with at least S.fieldcount slots (a stale fact could leave the
//     storage word a dictionary-mode MAP pointer — the addressing itself
//     would be wrong, not just the value).
//   - Facts are born on the TRUE edge of a cond_br whose condition is a
//     has_shape defined in the SAME block with no kill between its
//     definition and the branch ("fresh" — a compare separated from its
//     branch by a call would prove the shape held BEFORE the call, not
//     after).
//   - Facts die at every instruction whose effects include WRITE or CALL:
//     stores can transition/migrate the receiver, calls can run arbitrary
//     JS.  slot_store itself is a WRITE and kills — the "same-region store
//     provably doesn't transition" refinement is deliberately NOT modeled
//     (fail-closed; revisit with measurements).
//   - Facts never cross unwind edges (the throwing instruction may have
//     been mid-block, after arbitrary kills), so catch blocks start empty.
//   - Join = set intersection over incoming edges (a must-analysis).
//   - SSA immutability makes the VALUE part of a fact stable; only the
//     heap side (the object's header) can move, which is exactly what the
//     kill rule tracks.
//
// Number-tag facts (slot_store's repr proof) need no kill rule: has_tag
// tests the VALUE's own tag, and SSA values are immutable — dominance
// alone suffices (tagFactDominates below, the guardFactAt shape from
// optimize-guards generalized to either edge).
//
// The engine is shared with optimize-guards' shape-fact folding: the
// optimizer folds on the same facts the verifier re-derives, so a fold the
// optimizer gets wrong is a fold the verifier rejects (trust-free, the
// P3.4 discipline).

const SHAPE_KILL = Effect.WRITE | Effect.CALL;

export function shapeFactKey(valueId: number, shape: string): string {
    return `${valueId}|${shape}`;
}

function isShapeGuard(inst: Inst): boolean {
    return inst.op === "has_shape";
}

export interface ShapeFactAnalysis {
    // facts holding at entry of each reachable block
    blockIn: Map<Block, Set<string>>;
    // facts holding immediately before insts[uptoIndex] of `block`
    factsAt(block: Block, uptoIndex: number): Set<string>;
}

// forward must-dataflow of shape facts over the CFG.  Cheap bail: returns
// null when the function has no has_shape at all (every flag-off compile).
export function computeShapeFacts(fn: Func): ShapeFactAnalysis | null {
    const universe = new Set<string>();
    fn.forEachInst((inst) => {
        if (isShapeGuard(inst))
            universe.add(shapeFactKey(inst.operands[0]!.id, String(inst.imms["shape"])));
    });
    if (universe.size === 0) return null;

    const { rpo, reachable } = computeRPO(fn);
    const blockIn = new Map<Block, Set<string>>();
    for (const b of rpo) blockIn.set(b, b === fn.entry ? new Set() : new Set(universe));

    // transfer IN through the block's instructions (kills only; facts are
    // born on edges, not mid-block)
    const transfer = (b: Block, facts: Set<string>, uptoIndex: number): Set<string> => {
        let out = facts;
        const n = Math.min(uptoIndex, b.insts.length);
        for (let i = 0; i < n; i++) {
            const inst = b.insts[i]!;
            if ((opInfo(inst.op).effects & SHAPE_KILL) !== 0) {
                if (out.size > 0) out = new Set();
            }
        }
        return out;
    };

    // the fact a specific outgoing edge adds: the TRUE edge of a cond_br on
    // a same-block, still-fresh has_shape
    const edgeGen = (b: Block, targetIndex: number): string | null => {
        const term = b.terminator;
        if (!term || term.op !== "cond_br" || targetIndex !== 0) return null;
        const cond = term.operands[0]!;
        if (!isShapeGuard(cond) || cond.block !== b) return null;
        const gi = b.insts.indexOf(cond);
        if (gi < 0) return null;
        for (let i = gi + 1; i < b.insts.length; i++) {
            if ((opInfo(b.insts[i]!.op).effects & SHAPE_KILL) !== 0) return null; // stale
        }
        return shapeFactKey(cond.operands[0]!.id, String(cond.imms["shape"]));
    };

    let changed = true;
    while (changed) {
        changed = false;
        for (const b of rpo) {
            if (b === fn.entry) continue;
            let acc: Set<string> | null = null;
            for (const e of b.predEdges) {
                const p = e.inst.block!;
                if (!reachable.has(p)) continue;
                const t = e.inst.targets![e.targetIndex]!;
                let out: Set<string>;
                if (t.kind === "unwind") {
                    out = new Set(); // mid-block unwind: no facts survive
                } else {
                    out = new Set(transfer(p, blockIn.get(p) ?? new Set(), p.insts.length));
                    const gen = edgeGen(p, e.targetIndex);
                    if (gen) out.add(gen);
                }
                if (acc === null) acc = out;
                else for (const f of acc) if (!out.has(f)) acc.delete(f);
            }
            const next = acc ?? new Set<string>();
            const cur = blockIn.get(b)!;
            if (next.size !== cur.size || [...next].some((f) => !cur.has(f))) {
                blockIn.set(b, next);
                changed = true;
            }
        }
    }

    return {
        blockIn,
        factsAt: (block, uptoIndex) =>
            transfer(block, blockIn.get(block) ?? new Set(), uptoIndex),
    };
}

// value-intrinsic number proof: numbers by construction, no position
// involved.  The optimizer's foldProvenGuards legitimately deletes a
// has_tag whose value is proven this way (const numbers, box_f64, the
// always-number generic ops — optimize-guards' soundness inventory), so
// the slot_store rule must accept the same proofs or reject valid folds.
// Deliberately the INTRINSIC subset only: the optimizer's dominance-fact
// proofs never justify deleting a guard the store rule needs (a fold on a
// dominance fact leaves that dominating guard edge in place).
export function provenNumberIntrinsic(v: Inst, depth = 6): boolean {
    if (v.op === "const") return v.imms["kind"] === "number";
    if (v.op === "box_f64") return true;
    if (v.op === "mul" || v.op === "div" || v.op === "sub") return true;
    if (depth <= 0) return false;
    if (v.op === "add")
        return (
            provenNumberIntrinsic(v.operands[0]!, depth - 1) &&
            provenNumberIntrinsic(v.operands[1]!, depth - 1)
        );
    // a join whose every incoming is itself intrinsically a number (e.g.
    // `c ? 1 : 0` — const-number edges) is immutably a number.  This
    // mirrors provenNumberAt's blockparam case in optimize-guards: the
    // optimizer folds a has_tag over such a join, so the verifier must
    // accept the same proof for the slot_store it uncovers (the P4.2
    // proof-mismatch lesson, replayed — found by types-bornshapewrong1's
    // ternary-valued constructor store).
    if (v.op === "blockparam" && !v.isException && v.block && !v.block.isCatch) {
        const b = v.block;
        if (b.predEdges.length === 0) return false;
        const argIdx = b.argIndexOfParam(v);
        let anyProven = false;
        for (const e of b.predEdges) {
            const arg = e.inst.targets![e.targetIndex]!.args[argIdx];
            if (!arg) return false;
            if (arg === v) continue; // self-edge: vacuous
            if (!provenNumberIntrinsic(arg, depth - 1)) return false;
            anyProven = true;
        }
        return anyProven;
    }
    return false;
}

// is there a dominating (wantTrue ? true : false)-edge fact of
// `has_tag(v, "number")` at `block`?  Dominance-only: number-ness of an
// immutable SSA value is position-independent (see the inventory above).
export function tagFactDominates(
    v: Inst,
    wantTrue: boolean,
    block: Block,
    idom: Map<Block, Block>
): boolean {
    let b: Block = block;
    for (;;) {
        if (b.predEdges.length === 1) {
            const e = b.predEdges[0]!;
            if (
                e.inst.op === "cond_br" &&
                e.targetIndex === (wantTrue ? 0 : 1) &&
                e.inst.operands[0]!.op === "has_tag" &&
                e.inst.operands[0]!.imms["tag"] === "number" &&
                e.inst.operands[0]!.operands[0] === v
            )
                return true;
        }
        const n = idom.get(b);
        if (!n || n === b) return false;
        b = n;
    }
}

export function computeRPO(fn: Func): { rpo: Block[]; reachable: Set<Block> } {
    const entry = fn.entry!;
    const visited = new Set<Block>();
    const postorder: Block[] = [];
    // iterative dfs to keep the verifier usable on deep CFGs
    const stack = [{ block: entry, succIndex: 0 }];
    visited.add(entry);
    while (stack.length > 0) {
        const frame = stack[stack.length - 1]!;
        const succs = frame.block.succs();
        if (frame.succIndex < succs.length) {
            const s = succs[frame.succIndex++]!;
            if (!visited.has(s)) {
                visited.add(s);
                stack.push({ block: s, succIndex: 0 });
            }
        } else {
            postorder.push(frame.block);
            stack.pop();
        }
    }
    return { rpo: postorder.slice().reverse(), reachable: visited };
}

// Cooper/Harvey/Kennedy "A Simple, Fast Dominance Algorithm"
export function computeDominators(fn: Func, rpo: Block[]): Map<Block, Block> {
    const entry = fn.entry!;
    const index = new Map<Block, number>();
    rpo.forEach((b, i) => index.set(b, i));

    const idom = new Map<Block, Block>();
    idom.set(entry, entry);

    const intersect = (a: Block, b: Block): Block => {
        while (a !== b) {
            while (index.get(a)! > index.get(b)!) a = idom.get(a)!;
            while (index.get(b)! > index.get(a)!) b = idom.get(b)!;
        }
        return a;
    };

    let changed = true;
    while (changed) {
        changed = false;
        for (const b of rpo) {
            if (b === entry) continue;
            let newIdom: Block | null = null;
            for (const p of b.preds()) {
                if (!index.has(p)) continue; // unreachable pred
                if (!idom.has(p)) continue;
                newIdom = newIdom === null ? p : intersect(p, newIdom);
            }
            if (newIdom !== null && idom.get(b) !== newIdom) {
                idom.set(b, newIdom);
                changed = true;
            }
        }
    }
    return idom;
}

export function dominates(idom: Map<Block, Block>, a: Block, b: Block): boolean {
    // does block a dominate block b?
    let runner = b;
    for (;;) {
        if (runner === a) return true;
        const next = idom.get(runner);
        if (next === undefined || next === runner) return runner === a;
        runner = next;
    }
}

export function verifyFunction(fn: Func, mod?: Module): boolean {
    const vname = (v: Inst | null | undefined) => (v ? `%v${v.id}` : "<null>");
    const fail = (msg: string, inst?: Inst): never => {
        let where = "";
        if (inst) {
            const inst_str = printInst(inst, vname);
            where = ` at '${inst_str}'`;
        }
        throw new Error(`EIR verifier: fn @${fn.name}: ${msg}${where}`);
    };

    if (!fn.entry) fail("no entry block");

    const { rpo, reachable } = computeRPO(fn);
    const idom = computeDominators(fn, rpo);

    // per-block structural checks
    for (const b of fn.blocks) {
        if (!b.sealed) fail(`block ^${b.name} is not sealed`);
        if (!reachable.has(b)) continue; // ignore unreachable blocks beyond seal check

        let term: Inst | null = null;
        for (let i = 0; i < b.insts.length; i++) {
            const inst = b.insts[i]!;
            const info = opInfo(inst.op); // throws on unknown op
            if (info.arity >= 0 && inst.operands.length !== info.arity)
                fail(`'${inst.op}' has ${inst.operands.length} operands, wants ${info.arity}`, inst);
            if (isTerminator(inst)) {
                if (i !== b.insts.length - 1) fail(`terminator in the middle of ^${b.name}`, inst);
                term = inst;
            }
            if (inst.op === "blockparam") fail("blockparam in instruction stream", inst);
        }
        if (!term) {
            fail(`block ^${b.name} has no terminator`);
            continue;
        }

        // edge argument counts match target params (catch blocks' exception
        // param is produced by unwinding, not passed on the edge)
        if (term.targets) {
            for (const t of term.targets) {
                let expected = t.block.params.length;
                if (t.block.isCatch) {
                    if (t.kind !== "unwind")
                        fail(`non-unwind edge into catch block ^${t.block.name}`, term);
                    if (t.block.params.length === 0 || !t.block.params[0]!.isException)
                        fail(`catch block ^${t.block.name} missing its exception param`, term);
                    expected -= 1;
                } else if (t.kind === "unwind") {
                    fail(`unwind edge into non-catch block ^${t.block.name}`, term);
                }
                if (t.args.length !== expected)
                    fail(
                        `edge to ^${t.block.name} passes ${t.args.length} args, target wants ${expected}`,
                        term
                    );
                for (const a of t.args)
                    if (a === null || a === undefined)
                        fail(`edge to ^${t.block.name} has an unfilled argument`, term);
            }
        }
    }

    // def-dominates-use.  a value used as an operand must be defined in a
    // block that dominates the use block (params count as defined at block
    // entry; straight-line order enforced within a block).
    const instIndex = new Map<Inst, number>();
    for (const b of fn.blocks) {
        b.insts.forEach((inst, i) => instIndex.set(inst, i));
    }

    const checkUse = (val: Inst | null, userBlock: Block, userIdx: number, inst: Inst): void => {
        if (!val) {
            fail("null operand", inst);
            return;
        }
        if (val.removed) fail("use of removed block parameter", inst);
        const defBlock = val.block!;
        if (!reachable.has(defBlock)) fail("operand defined in unreachable block", inst);
        if (defBlock === userBlock) {
            if (val.op === "blockparam") return; // defined at entry of the block
            const defIdx = instIndex.get(val);
            if (defIdx === undefined || defIdx >= userIdx)
                fail(`operand %v${val.id} used before definition`, inst);
        } else {
            if (!dominates(idom, defBlock, userBlock))
                fail(
                    `operand %v${val.id} (def in ^${defBlock.name}) does not dominate use in ^${userBlock.name}`,
                    inst
                );
        }
    };

    for (const b of fn.blocks) {
        if (!reachable.has(b)) continue;
        b.insts.forEach((inst, i) => {
            for (const o of inst.operands) checkUse(o, b, i, inst);
            if (inst.targets) {
                for (const t of inst.targets) for (const a of t.args) checkUse(a, b, i, inst);
            }
        });
    }

    // typed-flow rules (the low tier).  f64/i1 values are raw machine values:
    //   - an op with a sig gets exactly what the sig says per slot ("f64"
    //     slots take only f64 values; "ejsval" slots take any boxed value,
    //     which excludes f64/i1);
    //   - an op without a sig takes only boxed values — with one exception:
    //     cond_br's condition may additionally be i1 (has_tag / f64_lt; the
    //     legacy "any"-typed condition sources to_boolean / prop_iter_next
    //     already emit their own machine i1);
    //   - branch-edge arguments must be boxed: block params are EjsValue
    //     phis in the emitter, so f64/i1 may NOT cross block boundaries.
    //     (Phase 3's guarded diamonds carry values across joins boxed.)
    //     Phase 3.4's ONE controlled exception: a param carrying the
    //     optimizer's rawJoin marker (Inst.rawJoin) is an f64-typed phi
    //     (double in the emitter) and takes exactly f64 arguments.  The
    //     marker is provenance, not trust — the full safety conditions
    //     are re-checked here, so the strict rule stays in force for
    //     every lowering-created edge: lowering never sets the marker,
    //     and an f64 param WITHOUT it is rejected outright.  i1 never
    //     crosses a block boundary under any rule.
    //     Exception-safety: a rawJoin param can never materialize an f64
    //     in a handler entry — catch blocks and unwind edges are
    //     rejected below — and an f64 value can never be *treated as* an
    //     ejsval in a handler (or anywhere), because every ejsval-taking
    //     slot and every boxed param rejects f64-typed operands/args.
    //     Phase 3.6's SECOND controlled exception: a specialized clone's
    //     ENTRY blockparam is f64 exactly when the function's sig types
    //     the matching formal f64 (env/this stay boxed); its `return`
    //     operand type must equal the sig's result; and every call_typed
    //     is re-checked against the callee Func's sig below (module-level,
    //     when the module is available).
    const isRaw = (t: string) => t === "f64" || t === "i1";
    const sigParamType = (b: Block, p: Inst): "any" | "f64" | null => {
        if (b !== fn.entry || !fn.sig) return null;
        const formalIdx = p.paramIndex - 2; // entry params: [%env, %this, ...formals]
        if (formalIdx < 0 || formalIdx >= fn.sig.formals.length) return null;
        return fn.sig.formals[formalIdx]!;
    };
    for (const b of fn.blocks) {
        if (!reachable.has(b)) continue;
        for (const p of b.params) {
            if (p.type === "f64" && sigParamType(b, p) === "f64") continue; // typed formal
            if (p.type === "f64") {
                if (!p.rawJoin)
                    fail(`f64 block param without the optimizer's rawJoin marker`, p);
                if (b.isCatch || p.isException)
                    fail(`rawJoin f64 param on a catch block / exception param`, p);
                for (const e of b.predEdges) {
                    const t = e.inst.targets![e.targetIndex]!;
                    if (t.kind === "unwind") fail(`rawJoin f64 param fed by an unwind edge`, p);
                    const a = t.args[b.argIndexOfParam(p)];
                    if (a && a.type !== "f64")
                        fail(`rawJoin f64 param receives a ${a.type} argument`, e.inst);
                }
            } else if (p.rawJoin) {
                fail(`rawJoin marker on a non-f64 block param`, p);
            }
        }
        for (const inst of b.insts) {
            const info = opInfo(inst.op);

            // branch-edge arguments (checked FIRST: the op-specific cases
            // below `continue` past the operand rules)
            if (inst.targets)
                for (const t of inst.targets)
                    t.args.forEach((a, i) => {
                        if (!a) return;
                        const param = t.block.params[i + (t.block.isCatch ? 1 : 0)];
                        if (a.type === "f64") {
                            if (!param || !param.rawJoin || param.type !== "f64")
                                fail(
                                    `edge to ^${t.block.name} passes a raw ${a.type} value; block arguments must be boxed`,
                                    inst
                                );
                        } else if (a.type === "i1") {
                            fail(
                                `edge to ^${t.block.name} passes a raw ${a.type} value; block arguments must be boxed`,
                                inst
                            );
                        } else if (param && param.type === "f64") {
                            fail(
                                `edge to ^${t.block.name} passes a boxed value to an f64 param`,
                                inst
                            );
                        }
                    });

            // Phase 3.6: call_typed is typed by its CALLEE's sig, which a
            // per-op table can't express.  operand 0 (env) stays boxed;
            // the argument slots must match the callee's formals exactly,
            // and the instruction's stamped result type must equal the
            // callee sig's result.  Without a module (standalone
            // verifyFunction) the callee can't be resolved; the boxed-env
            // and no-i1 rules still hold.
            if (inst.op === "call_typed") {
                const calleeName = inst.imms["fn"] as string;
                const callee = mod ? mod.functions.find((f) => f.name === calleeName) : undefined;
                if (mod) {
                    if (!callee) fail(`call_typed to unknown function '${calleeName}'`, inst);
                    if (!callee!.sig) fail(`call_typed to un-sigged function '${calleeName}'`, inst);
                    const formals = callee!.sig!.formals;
                    if (inst.operands.length - 1 !== formals.length)
                        fail(
                            `call_typed passes ${inst.operands.length - 1} args, ` +
                                `callee sig wants ${formals.length}`,
                            inst
                        );
                    const wantResult = callee!.sig!.result === "f64" ? "f64" : "any";
                    if (inst.type !== wantResult)
                        fail(`call_typed result type ${inst.type} != callee sig ${wantResult}`, inst);
                }
                inst.operands.forEach((o, idx) => {
                    if (idx === 0) {
                        if (isRaw(o.type))
                            fail(`call_typed env operand must be boxed, got ${o.type}`, inst);
                        return;
                    }
                    if (o.type === "i1") fail(`call_typed operand ${idx} may not be i1`, inst);
                    if (callee && callee.sig) {
                        const want = callee.sig.formals[idx - 1]!;
                        if (want === "f64" ? o.type !== "f64" : isRaw(o.type))
                            fail(
                                `call_typed operand ${idx} wants ${want}, got ${o.type}`,
                                inst
                            );
                    }
                });
                continue;
            }
            // Phase 3.6: a sigged function's `return` must produce exactly
            // the sig's result type (f64 result -> raw f64 operand)
            if (inst.op === "return" && fn.sig && fn.sig.result === "f64") {
                const o = inst.operands[0]!;
                if (o.type !== "f64")
                    fail(`return in an f64-result function got ${o.type}`, inst);
                continue;
            }

            inst.operands.forEach((o, idx) => {
                const want = info.sig ? info.sig.params[idx] : undefined;
                if (want === "f64") {
                    if (o.type !== "f64")
                        fail(`'${inst.op}' operand ${idx} wants f64, got ${o.type}`, inst);
                } else if (want === "ejsval") {
                    if (isRaw(o.type))
                        fail(`'${inst.op}' operand ${idx} wants a boxed value, got ${o.type}`, inst);
                } else if (inst.op === "cond_br" && idx === 0) {
                    if (o.type === "f64") fail("cond_br condition may not be f64", inst);
                } else if (isRaw(o.type)) {
                    fail(`'${inst.op}' operand ${idx} may not be ${o.type}`, inst);
                }
            });
        }
    }

    // --- shapes-plan P4.3: shape-guarded slot access -----------------------
    // Every slot op must sit under an un-killed dominating has_shape fact on
    // the same value for the same shape (see the effect-kill inventory at the
    // top of this file); stores additionally prove the stored value's tag
    // matches the field repr, so compiled stores never owe a transition.
    // With a module in hand, imms are checked against the module shape table
    // (bounds, repr identity, known key).
    let shapeFacts: ShapeFactAnalysis | null | undefined;
    for (const b of fn.blocks) {
        if (!reachable.has(b)) continue;
        b.insts.forEach((inst, i) => {
            const isSlotOp = inst.op === "slot_load" || inst.op === "slot_store";
            const isBornOp = inst.op === "make_object_shaped" || inst.op === "fill_object_shaped";
            if (!isSlotOp && !isBornOp && inst.op !== "has_shape") return;
            const shapeImm = String(inst.imms["shape"]);
            const fields = mod ? mod.shapes.get(shapeImm) : undefined;
            if (mod && !fields)
                fail(`'${inst.op}' names unknown module shape '${shapeImm}'`, inst);
            if (isBornOp) {
                // shapes-plan P4.4: operand count must equal the shape's
                // field count (+1 receiver for fill), at least one field —
                // an empty born shape is a plain make_object, not this op.
                const nvals =
                    inst.op === "make_object_shaped"
                        ? inst.operands.length
                        : inst.operands.length - 1;
                if (fields && nvals !== fields.length)
                    fail(
                        `'${inst.op}' has ${nvals} values for shape '${shapeImm}' (${fields.length} fields)`,
                        inst
                    );
                if (nvals < 1) fail(`'${inst.op}' must install at least one field`, inst);
                if (inst.op === "fill_object_shaped") {
                    // the receiver must be proven EMPTY-shaped here: the
                    // batched prefix is only equivalent to the sequential
                    // stores on an object with no fields yet (an un-killed
                    // has_shape(recv, "") fact — same engine as slot ops)
                    if (shapeFacts === undefined) shapeFacts = computeShapeFacts(fn);
                    const facts = shapeFacts ? shapeFacts.factsAt(b, i) : new Set<string>();
                    if (!facts.has(shapeFactKey(inst.operands[0]!.id, "")))
                        fail(
                            `'fill_object_shaped' is not covered by an un-killed has_shape fact ` +
                                `for the empty shape on its receiver`,
                            inst
                        );
                }
                return;
            }
            if (!isSlotOp) return;

            const slot = inst.imms["slot"];
            const repr = inst.imms["repr"];
            if (typeof slot !== "number" || slot < 0 || !Number.isInteger(slot))
                fail(`'${inst.op}' has a malformed slot immediate`, inst);
            if (repr !== "boxed" && repr !== "f64")
                fail(`'${inst.op}' has a malformed repr immediate`, inst);
            if (fields) {
                if ((slot as number) >= fields.length)
                    fail(
                        `'${inst.op}' slot ${slot} out of bounds for shape '${shapeImm}' (${fields.length} fields)`,
                        inst
                    );
                if (fields[slot as number]!.repr !== repr)
                    fail(
                        `'${inst.op}' repr "${String(repr)}" != shape field repr "${fields[slot as number]!.repr}"`,
                        inst
                    );
            }

            if (shapeFacts === undefined) shapeFacts = computeShapeFacts(fn);
            const facts = shapeFacts ? shapeFacts.factsAt(b, i) : new Set<string>();
            if (!facts.has(shapeFactKey(inst.operands[0]!.id, shapeImm)))
                fail(
                    `'${inst.op}' is not covered by an un-killed has_shape fact for shape '${shapeImm}'`,
                    inst
                );

            if (inst.op === "slot_store") {
                const val = inst.operands[1]!;
                const proven =
                    repr === "f64"
                        ? tagFactDominates(val, true, b, idom) || provenNumberIntrinsic(val)
                        : tagFactDominates(val, false, b, idom);
                if (!proven)
                    fail(
                        `slot_store lacks a dominating has_tag(number)=${repr === "f64"} fact ` +
                            `on its value for repr "${String(repr)}"`,
                        inst
                    );
            }
        });
    }

    return true;
}

export function verifyModule(mod: Module): boolean {
    for (const fn of mod.functions) verifyFunction(fn, mod);
    return true;
}
