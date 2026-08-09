/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// Generator state-machine lowering (docs/generator-eir-plan.md, G1).
//
// A marked generator body (Func.genBody, lowered with gen_yield ops)
// becomes a resume-dispatch state machine: the runtime re-calls the
// body closure as body(gen, mode, sent) and every yield suspends by
// RETURNING — no machine stacks, no ucontexts.
//
//   - Suspend: each gen_yield k rewrites to
//       generator_eir_suspend(gen, k); return <yielded value>
//     The runtime distinguishes this from a completion return by the
//     suspended flag the call sets.
//   - Dispatch: the entry block gains a state chain —
//       st = generator_eir_state(gen)
//       st == k  ->  resume_k   (switch_index_eq boxed-bit compares)
//       else     ->  the original entry code (state 0)
//   - Resume: resume_k dispatches on mode: throw rethrows the sent
//     value AT the yield point (reusing the yield's unwind edge, so
//     enclosing try regions apply); return throws the return sentinel
//     (the desugar's wrapper catch completes with the value); next
//     branches to the yield's continuation.  The yield's RESULT is
//     simply the sent param — uses are rewritten to it directly.
//   - Persistence: bindings already live in the closure env (scopes.ts
//     force-captures a generator body's bindings); the env is stored on
//     the generator object right after its creation
//     (generator_eir_set_env) and every value live across a yield that
//     is NOT env-resident — ANF temps, loop-env references — demotes
//     into extra env slots: stored at its definition, reloaded at every
//     use through a per-block generator_eir_get_env.  The reloads keep
//     resume paths valid (the original defs never execute there); the
//     state-0 redundancy is LLVM's to clean.
//
// Exception routing needs no re-establishment: EIR unwind edges are
// per-instruction, so a dispatch branch into a mid-try block leaves
// the region's handlers exactly as lowered.

import { Func, Block, Inst, Module, replaceAllUses, usersOf } from "./ir";

const MODE_THROW = 1;
const MODE_RETURN = 2;

export function lowerGeneratorBodies(module: Module): number {
    let n = 0;
    for (const fn of module.functions) {
        if (!fn.genBody) continue;
        if (transformOne(fn)) n++;
    }
    return n;
}

// values live immediately after each gen_yield (for invoke-form yields
// this is the union over both successor edges, unwind included — the
// delegation loop's close handler reads pre-yield values).
function liveAfterYields(fn: Func, yields: Inst[]): Map<Inst, Set<Inst>> {
    const liveIn = new Map<Block, Set<Inst>>();
    const liveOut = new Map<Block, Set<Inst>>();
    for (const b of fn.blocks) {
        liveIn.set(b, new Set());
        liveOut.set(b, new Set());
    }
    const usesOf = (inst: Inst, f: (v: Inst) => void) => {
        for (const o of inst.operands) f(o);
        if (inst.targets) for (const t of inst.targets) for (const a of t.args) if (a) f(a);
    };
    let changed = true;
    while (changed) {
        changed = false;
        for (let bi = fn.blocks.length - 1; bi >= 0; bi--) {
            const b = fn.blocks[bi]!;
            const out = liveOut.get(b)!;
            const before = out.size;
            const last = b.insts[b.insts.length - 1];
            if (last && last.targets) {
                for (const t of last.targets) {
                    const sIn = liveIn.get(t.block);
                    if (!sIn) continue;
                    for (const v of sIn) out.add(v);
                    for (const p of t.block.params) out.delete(p);
                }
            }
            if (out.size !== before) changed = true;

            const live = new Set(out);
            for (let i = b.insts.length - 1; i >= 0; i--) {
                const inst = b.insts[i]!;
                live.delete(inst);
                usesOf(inst, (v) => live.add(v));
            }
            for (const p of b.params) live.delete(p);
            const inSet = liveIn.get(b)!;
            const inBefore = inSet.size;
            for (const v of live) inSet.add(v);
            if (inSet.size !== inBefore) changed = true;
        }
    }

    const result = new Map<Inst, Set<Inst>>();
    for (const b of fn.blocks) {
        const live = new Set(liveOut.get(b)!);
        for (let i = b.insts.length - 1; i >= 0; i--) {
            const inst = b.insts[i]!;
            if (inst.op === "gen_yield") result.set(inst, new Set(live));
            live.delete(inst);
            usesOf(inst, (v) => live.add(v));
        }
    }
    for (const y of yields) if (!result.has(y)) result.set(y, new Set());
    return result;
}

function transformOne(fn: Func): boolean {
    const entry = fn.entry!;
    // entry params: [%env, %this, gen, mode, sent] — the desugar's
    // resume-protocol arrow signature
    if (entry.params.length < 5)
        throw new Error(`gen-lower: generator body '${fn.name}' lacks the resume params`);
    const genP = entry.params[2]!;
    const modeP = entry.params[3]!;
    const sentP = entry.params[4]!;

    const yields: Inst[] = [];
    for (const b of fn.blocks) for (const i of b.insts) if (i.op === "gen_yield") yields.push(i);
    // a yield-free body completes on its first resume; the plain
    // function already implements that
    if (yields.length === 0) return false;

    const mk = (op: string, operands: Inst[], imms?: Record<string, unknown>) =>
        new Inst(fn, op, operands, (imms || {}) as Inst["imms"]);
    const insertAt = (b: Block, idx: number, inst: Inst) => {
        inst.block = b;
        b.insts.splice(idx, 0, inst);
    };

    // --- demotion set ----------------------------------------------------
    const liveAfter = liveAfterYields(fn, yields);
    const entryParams = new Set(entry.params);
    const demoted: Inst[] = [];
    const demotedSet = new Set<Inst>();
    for (const y of yields) {
        for (const v of liveAfter.get(y)!) {
            if (entryParams.has(v) || demotedSet.has(v)) continue;
            if (v.type !== "any")
                throw new Error(
                    `gen-lower: raw ${v.type} value live across a yield in '${fn.name}'`
                );
            demotedSet.add(v);
            demoted.push(v);
        }
    }

    // --- the persistent env ----------------------------------------------
    // the fn env is the first make_env of the entry block (the lowering's
    // entry protocol emits it before any loop can); loop envs live in
    // later blocks
    let fnEnvMake: Inst | null = null;
    for (const i of entry.insts) {
        if (i.op === "make_env") {
            fnEnvMake = i;
            break;
        }
    }
    // snapshot users before we start inserting instructions of our own
    const userSnapshot = new Map<Inst, Inst[]>();
    for (const v of demoted) userSnapshot.set(v, usersOf(fn, v));

    let slotBase = 0;
    if (fnEnvMake) slotBase = Number(fnEnvMake.imms["size"]);
    else if (demoted.length > 0) {
        fnEnvMake = mk("make_env", [], { size: 0 });
        insertAt(entry, 0, fnEnvMake);
    }
    if (fnEnvMake) {
        fnEnvMake.imms["size"] = slotBase + demoted.length;
        const idx = entry.insts.indexOf(fnEnvMake);
        insertAt(
            entry,
            idx + 1,
            mk("call_runtime", [genP, fnEnvMake], { name: "generator_eir_set_env", void: true })
        );
    }
    const slotOf = new Map<Inst, number>();
    demoted.forEach((v, i) => slotOf.set(v, slotBase + i));

    // per-block env access: the block holding fnEnvMake uses it directly;
    // every other block reloads from the generator once at its top
    const blockEnv = new Map<Block, Inst>();
    const envFor = (b: Block): Inst => {
        if (fnEnvMake && b === fnEnvMake.block) return fnEnvMake;
        let e = blockEnv.get(b);
        if (!e) {
            e = mk("call_runtime", [genP], { name: "generator_eir_get_env" });
            insertAt(b, 0, e);
            blockEnv.set(b, e);
        }
        return e;
    };

    // --- demote: store at def, reload at every use ------------------------
    for (const v of demoted) {
        const slot = slotOf.get(v)!;
        // an invoke-form def (a call — or a yield — inside a try region)
        // terminates its block; its store goes at the top of the NORMAL
        // successor, the first point the value exists.  For a demoted
        // gen_yield this is also exactly right: the continuation is only
        // ever reached by a mode-0 resume, and the later
        // replaceAllUses(y, sent) turns this store into "persist the
        // value sent at THIS resume".
        let storeBlock: Block;
        let atTop: boolean;
        if (v.targets && v.targets.length > 0) {
            storeBlock = v.targets.find((t) => t.kind !== "unwind")!.block;
            atTop = true;
        } else {
            storeBlock = v.block!;
            atTop = v.op === "blockparam";
        }
        const env = envFor(storeBlock);
        const store = mk("env_store", [env, v], { slot });
        // top-of-block stores sit after the block's env reload when one
        // was just inserted there
        const defIdx = atTop
            ? env.block === storeBlock
                ? storeBlock.insts.indexOf(env) + 1
                : 0
            : storeBlock.insts.indexOf(v) + 1;
        insertAt(storeBlock, defIdx, store);

        for (const u of userSnapshot.get(v)!) {
            const ub = u.block!;
            const load = mk("env_load", [envFor(ub)], { slot });
            insertAt(ub, ub.insts.indexOf(u), load);
            for (let i = 0; i < u.operands.length; i++)
                if (u.operands[i] === v) u.operands[i] = load;
            if (u.targets)
                for (const t of u.targets)
                    for (let i = 0; i < t.args.length; i++) if (t.args[i] === v) t.args[i] = load;
        }
    }

    // --- rewrite each yield into suspend/return + a resume block ----------
    const resumeBlocks: Block[] = [];
    yields.forEach((y, yi) => {
        const k = yi + 1;
        const B = y.block!;
        const idx = B.insts.indexOf(y);

        let contBlock: Block;
        let contArgs: (Inst | null)[] = [];
        let unwindTarget: { block: Block; args: (Inst | null)[] } | null = null;
        if (y.targets && y.targets.length > 0) {
            // invoke form: the continuation is the normal target
            const normal = y.targets.find((t) => t.kind !== "unwind")!;
            const unwind = y.targets.find((t) => t.kind === "unwind");
            contBlock = normal.block;
            contArgs = normal.args.slice();
            if (unwind) unwindTarget = { block: unwind.block, args: unwind.args.slice() };
            // B no longer branches anywhere through y
            for (const t of y.targets)
                t.block.predEdges = t.block.predEdges.filter((e) => e.inst !== y);
        } else {
            contBlock = new Block(fn, "gen_cont");
            fn.blocks.push(contBlock);
            contBlock.sealed = true;
            const tail = B.insts.splice(idx + 1);
            for (const i of tail) i.block = contBlock;
            contBlock.insts = tail;
        }

        // suspend: set the state, return the yielded value
        const kConst = mk("const", [], { kind: "number", value: k });
        const suspend = mk("call_runtime", [genP, kConst], {
            name: "generator_eir_suspend",
            void: true,
        });
        const ret = mk("return", [y.operands[1]!]);
        B.insts.splice(idx, 1, kConst, suspend, ret);
        kConst.block = suspend.block = ret.block = B;

        // the yield's value on the (only) path that reaches the
        // continuation — a mode-0 resume — is the sent param
        replaceAllUses(fn, y, sentP);
        for (let i = 0; i < contArgs.length; i++) if (contArgs[i] === y) contArgs[i] = sentP;
        if (unwindTarget)
            for (let i = 0; i < unwindTarget.args.length; i++)
                if (unwindTarget.args[i] === y) unwindTarget.args[i] = sentP;

        // resume_k: mode dispatch
        const R = new Block(fn, "gen_resume");
        const chk = new Block(fn, "gen_resume_chk");
        const thr = new Block(fn, "gen_resume_throw");
        const retn = new Block(fn, "gen_resume_return");
        const cont = new Block(fn, "gen_resume_next");
        for (const b of [R, chk, thr, retn, cont]) {
            fn.blocks.push(b);
            b.sealed = true;
        }

        const c1 = mk("switch_index_eq", [modeP], { index: MODE_THROW });
        const br1 = mk("cond_br", [c1]);
        br1.addTarget(thr, []);
        br1.addTarget(chk, []);
        for (const i of [c1, br1]) {
            i.block = R;
            R.insts.push(i);
        }

        const c2 = mk("switch_index_eq", [modeP], { index: MODE_RETURN });
        const br2 = mk("cond_br", [c2]);
        br2.addTarget(retn, []);
        br2.addTarget(cont, []);
        for (const i of [c2, br2]) {
            i.block = chk;
            chk.insts.push(i);
        }

        // gen.throw(v): rethrow at the yield point; the yield's unwind
        // edge (if any) routes it into the enclosing handler
        const t1 = mk("throw", [sentP]);
        if (unwindTarget) t1.addTarget(unwindTarget.block, unwindTarget.args.slice(), "unwind");
        t1.block = thr;
        thr.insts.push(t1);

        // gen.return(v): throw the return sentinel through the body —
        // finally blocks run, the wrapper catch completes with the value
        const sv = mk("call_runtime", [], { name: "generator_eir_sentinel" });
        const t2 = mk("throw", [sv]);
        if (unwindTarget) t2.addTarget(unwindTarget.block, unwindTarget.args.slice(), "unwind");
        for (const i of [sv, t2]) {
            i.block = retn;
            retn.insts.push(i);
        }

        const bcont = mk("br", []);
        bcont.addTarget(contBlock, contArgs as Inst[]);
        bcont.block = cont;
        cont.insts.push(bcont);

        resumeBlocks.push(R);
    });

    // --- entry dispatch ---------------------------------------------------
    const start0 = new Block(fn, "gen_start");
    fn.blocks.push(start0);
    start0.sealed = true;
    start0.insts = entry.insts;
    for (const i of start0.insts) i.block = start0;
    entry.insts = [];
    // successor pred-edges keep pointing at the moved terminator; its
    // block backref moved with it, so nothing else to fix

    const st = mk("call_runtime", [genP], { name: "generator_eir_state" });
    st.block = entry;
    entry.insts.push(st);
    let cur = entry;
    resumeBlocks.forEach((R, i) => {
        const k = i + 1;
        const ck = mk("switch_index_eq", [st], { index: k });
        const br = mk("cond_br", [ck]);
        br.addTarget(R, []);
        const next = i === resumeBlocks.length - 1 ? start0 : new Block(fn, "gen_chk");
        if (next !== start0) {
            fn.blocks.push(next);
            next.sealed = true;
        }
        br.addTarget(next, []);
        for (const inst of [ck, br]) {
            inst.block = cur;
            cur.insts.push(inst);
        }
        cur = next;
    });

    return true;
}
