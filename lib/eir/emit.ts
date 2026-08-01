/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// EIR -> LLVM emission.
//
// Every EIR value becomes an LLVM SSA value; block arguments become phis;
// invoke-style instructions (normal/unwind targets) become llvm invokes
// landing in the catch block's landingpad.  The only allocas are the arg
// scratch area and the &this slot that the runtime calling convention
// requires -- locals never touch memory, and mem2reg has nothing to do.
//
// The emitter borrows the active LLVMIRVisitor's infrastructure (llvm
// module, abi, runtime interface, atom/string-literal machinery) through
// the VisitorSurface interface below.

import * as llvm from "@llvm";
import * as types from "../types";
import * as consts from "../consts";
import type { ABI } from "../abi";
import type { RuntimeInterface } from "../runtime";
import type { Module as EIRModule, Func, Block, Inst, Target } from "./ir";
import { passes } from "../pass-config";
import { computeSpilledValues } from "./liveness";

const ir = llvm.IRBuilder;

// the slice of LLVMIRVisitor the emitter uses (compiler.ts implements it)
export interface VisitorSurface {
    currentFunction: llvm.EjsFunction | null;
    ejs_runtime: RuntimeInterface;
    ejs_binops: Record<string, llvm.EjsFunction>;
    ejs_globals: Record<string, llvm.GlobalVariable>;
    import_module_globals: Map<string, llvm.GlobalVariable>;
    this_module_global: llvm.GlobalVariable;
    getAtom(str: string): llvm.Value;
    createEjsValueLoad(value: llvm.Value, name: string): llvm.Value;
    emitEjsvalFromPtr(ptr: llvm.Value, prefix: string): llvm.Value;
    isNumber(val: llvm.Value): llvm.Value;
    // the low tier's NaN-box transfers (implemented beside isNumber in
    // compiler.ts so all target-layout knowledge stays in one place)
    unboxDouble(val: llvm.Value): llvm.Value;
    boxDouble(dbl: llvm.Value): llvm.Value;
    // shape-guard NaN-box tests (beside isNumber for the same reason): the object
    // tag test, the payload->EJSObject* reinterpretation (valid only under
    // a passed isObject), and the module's interned shape-index global
    isObject(val: llvm.Value): llvm.Value;
    objectPointer(val: llvm.Value): llvm.Value;
    // the inline half of the write barrier — "is this
    // value's payload in the nursery range" (layout knowledge lives in
    // compiler.ts with the other NaN-box tests)
    emitYoungCheck(val: llvm.Value): llvm.Value;
    // i1: the runtime's accessor epoch is still zero (one global load +
    // compare; the global lives beside the other runtime seams)
    emitAccessorEpochCheck(): llvm.Value;
    // inline nursery bump allocation for closure envs
    emitEnvAllocInline(n: number, slowCall: () => llvm.Value): llvm.Value;
    // the gc-frame record (precise relocatable JS roots) and
    // inline env slot addressing (all layout knowledge in compiler.ts)
    emitGCFrameLink(frame: llvm.Value, nslots: number, undef: llvm.Value): void;
    emitGCFrameUnlink(frame: llvm.Value): void;
    emitGCFrameRelink(frame: llvm.Value): void;
    gcFrameSlotPtr(frame: llvm.Value, i: number): llvm.Value;
    emitEnvSlotRef(env: llvm.Value, slot: number): llvm.Value;
    moduleShapeGlobal(
        key: string,
        fields: { name: string; repr: string }[]
    ): llvm.GlobalVariable;
    loadBoolEjsValue(n: boolean): llvm.Value;
    loadDoubleEjsValue(n: number): llvm.Value;
    loadNullEjsValue(): llvm.Value;
    loadUndefinedEjsValue(): llvm.Value;
}

// EIR opcode -> the operator key used by runtime.ts's binop interface
const binop_for_op: Record<string, string | undefined> = {
    add: "+",
    sub: "-",
    mul: "*",
    div: "/",
    mod: "%",
    exp: "**",
    lt: "<",
    le: "<=",
    gt: ">",
    ge: ">=",
    loose_eq: "==",
    loose_neq: "!=",
    strict_eq: "===",
    strict_neq: "!==",
    bitand: "&",
    bitor: "|",
    bitxor: "^",
    shl: "<<",
    shr: ">>",
    ushr: ">>>",
    instanceof: "instanceof",
    in: "in",
};

const unop_for_op: Record<string, string | undefined> = {
    logical_not: "!",
    neg: "-",
    unary_plus: "+",
    bitnot: "~",
    typeof: "typeof",
};

let mangle_gen = 0;

// reachable blocks of `fn` in reverse postorder (entry first).  iterative
// DFS: block counts are small, but the self-hosted stack isn't deep.
function rpoBlocks(fn: Func): Block[] {
    const entry = fn.entry!;
    const visited = new Set<Block>([entry]);
    const post: Block[] = [];
    const stack = [{ block: entry, next: 0 }];
    while (stack.length > 0) {
        const frame = stack[stack.length - 1]!;
        const last = frame.block.insts[frame.block.insts.length - 1];
        const targets = (last && last.targets) || [];
        if (frame.next < targets.length) {
            const succ = targets[frame.next++]!.block;
            if (!visited.has(succ)) {
                visited.add(succ);
                stack.push({ block: succ, next: 0 });
            }
        } else {
            post.push(frame.block);
            stack.pop();
        }
    }
    post.reverse();
    return post;
}

export class EIREmitter {
    // the active LLVMIRVisitor: its module, abi, runtime/binop
    // interfaces, getAtom, and globals
    v: VisitorSurface;
    abi: ABI;
    module: llvm.Module;
    // per-module state
    llvm_fns!: Map<string, llvm.EjsFunction>;
    eirModule!: EIRModule;
    // per-function state (reset in emitFunction)
    eirFn!: Func;
    llvmFn!: llvm.EjsFunction;
    values!: Map<Inst, llvm.Value>;
    blocks!: Map<Block, llvm.BasicBlock>;
    phis!: Map<Inst, llvm.PhiNode>;
    fn_argc!: llvm.Value;
    fn_args_ptr!: llvm.Value;
    fn_this_ptr!: llvm.Value;
    fn_new_target!: llvm.Value;
    scratch: llvm.AllocaInst | null = null;
    // the slot-array env loaded by the most recent slotRef —
    // shaped stores must remember the ENV (the storage owner), not the
    // object whose Scan only holds the env reference
    scratch_type: llvm.Type | null = null;
    this_slot!: llvm.AllocaInst;
    // values live across a safepoint are DEMOTED to
    // gc-frame slots — stored once where they're defined, LOADED at
    // every use (val() intercepts).  Every load is dominated by its
    // def's store; LLVM CSEs redundant loads between safepoints but
    // cannot forward across one (the frame escapes via the chain), so a
    // collector rewrite is always observed.
    gc_frame: llvm.AllocaInst | null = null;
    gc_frame_slots: Map<Inst, number> | null = null;

    constructor(visitor: VisitorSurface & { abi: ABI; module: llvm.Module }) {
        this.v = visitor;
        this.abi = visitor.abi;
        this.module = visitor.module;
    }

    // declare + define every function in an EIR module; returns a Map of
    // eir function name -> llvm.Function
    emitModule(eirModule: EIRModule): Map<string, llvm.EjsFunction> {
        this.eirModule = eirModule;
        let saved_insert = ir.getInsertBlock();

        let fns = new Map();
        for (let fn of eirModule.functions) {
            if (fns.has(fn.name))
                throw new Error(`EIR emit: duplicate function name '${fn.name}' in module`);
            let llvm_name = `_ejs_eir_${fn.name.replace(/[^A-Za-z0-9_]/g, "_")}_${mangle_gen++}`;
            let llvm_fn;
            if (fn.sig) {
                // specialized clone: a native unboxed signature
                // — (double...) -> double — instead of the runtime's boxed
                // (env, this*, argc, argv*, newTarget) convention.  The
                // specialization post-checks guarantee the clone touches
                // neither env nor `this`, so NEITHER gets an argument slot
                // (the EIR-level %env operand of call_typed is simply not
                // emitted) — one less register at every out-of-line call
                // site, and LLVM's O2 pipeline demonstrably does not
                // dead-arg-eliminate it for us.  Internal-linkage,
                // direct-call-only (call_typed), so no takes_builtins and
                // no closure-dispatch interop; this is what finally lets
                // LLVM inline and scalar-optimize through the call.
                const param_types = fn.sig.formals.map((f) =>
                    f === "f64" ? types.Double : types.EjsValue
                );
                const ret_type = fn.sig.result === "f64" ? types.Double : types.EjsValue;
                llvm_fn = this.abi.createFunction(this.module, llvm_name, ret_type, param_types);
            } else {
                llvm_fn = types.takes_builtins(
                    this.abi.createFunction(
                        this.module,
                        llvm_name,
                        this.abi.ejs_return_type,
                        this.abi.ejs_params.map((p) => p.llvm_type)
                    )
                );
            }
            llvm_fn.setInternalLinkage();
            fns.set(fn.name, llvm_fn);
        }
        this.llvm_fns = fns;

        for (let fn of eirModule.functions) this.emitFunction(fn, fns.get(fn.name)!);

        if (saved_insert) ir.setInsertPoint(saved_insert);
        return fns;
    }

    emitFunction(eirFn: Func, llvmFn: llvm.EjsFunction): llvm.EjsFunction {
        this.eirFn = eirFn;
        this.llvmFn = llvmFn;
        this.values = new Map(); // eir Inst -> llvm value
        this.blocks = new Map(); // eir Block -> llvm BasicBlock
        this.phis = new Map(); // eir blockparam Inst -> llvm phi

        // legacy machinery (getAtom / literal loads) expects these on the
        // visitor's current function
        let saved_function = this.v.currentFunction;
        this.v.currentFunction = llvmFn;

        let entry_bb = new llvm.BasicBlock("entry", llvmFn);
        ir.setInsertPoint(entry_bb);
        llvmFn.entry_bb = entry_bb; // literal allocas / legacy helpers want this
        llvmFn.literalAllocas = Object.create(null);

        const args = llvmFn.args;
        // specialized clones have no env/this*/argc/argv*/newTarget — their
        // llvm args are the formals alone; the specialization pass
        // guarantees no op that needs the frame values survives (frame
        // ops, env/this uses, and generic returns all discard a clone)
        const env = eirFn.sig ? undefined! : args[0]!;
        const this_ptr = eirFn.sig ? undefined! : args[1]!;
        const argc = eirFn.sig ? undefined! : args[2]!;
        const args_ptr = eirFn.sig ? undefined! : args[3]!;
        // rest_args / args_obj / construct_super / new_target need the raw
        // calling-convention values
        this.fn_argc = argc;
        this.fn_args_ptr = args_ptr;
        this.fn_this_ptr = this_ptr;
        this.fn_new_target = eirFn.sig ? undefined! : args[4]!;

        // scratch space for outgoing call arguments, and a slot for passing
        // &this to the runtime's calling convention
        let max_args = this.maxOutgoingArgs(eirFn);
        this.scratch = null;
        this.scratch_type = null;
        if (max_args > 0) {
            this.scratch_type = llvm.ArrayType.get(types.EjsValue, max_args);
            this.scratch = ir.createAlloca(this.scratch_type, "args_scratch");
            this.scratch.setAlignment(8);
        }
        this.this_slot = ir.createAlloca(types.EjsValue, "this_slot");
        this.this_slot.setAlignment(8);

        // values live across safepoints get frame slots
        this.gc_frame = null;
        this.gc_frame_slots = null;
        if (passes().gcFrames) {
            const spilled = computeSpilledValues(eirFn);
            if (spilled) {
                const slots = new Map<Inst, number>();
                let i = 0;
                for (const v of spilled) slots.set(v, i++);
                this.gc_frame_slots = slots;
                this.gc_frame = ir.createAlloca(
                    llvm.ArrayType.get(types.Int64, 2 + slots.size),
                    "gc_frame"
                );
                this.gc_frame.setAlignment(8);
            }
        }

        // emit blocks in reverse postorder: a def's block always precedes
        // its uses' blocks (dominators come first in any RPO), so the
        // values map is filled before it's read.  block *creation* order in
        // the lowerer doesn't have that property (e.g. switch bodies are
        // created before their test chain).  unreachable blocks are dropped
        // entirely — nothing branches to them, and their phis would be
        // invalid (zero incoming edges).
        let order = rpoBlocks(eirFn);

        // create llvm blocks for every reachable eir block, and phis for
        // their params
        for (let b of order) {
            let bb = new llvm.BasicBlock(b.name, llvmFn);
            this.blocks.set(b, bb);
        }
        for (let b of order) {
            if (b === eirFn.entry) continue;
            ir.setInsertPoint(this.blocks.get(b)!);
            for (let p of b.params) {
                if (p.isException) continue; // materialized by the landingpad below
                // rawJoin params (the raw-join pass) carry raw doubles;
                // everything else is an EjsValue phi (the P2 boxed rule)
                let phi_type = p.type === "f64" ? types.Double : types.EjsValue;
                let phi = ir.createPhi(phi_type, b.predEdges.length, `p_${p.id}`);
                this.phis.set(p, phi);
                this.values.set(p, phi);
            }
            if (b.isCatch) this.emitCatchPrologue(b);
        }

        // entry prologue: bind the eir entry params.  this happens in a
        // separate block because entry_bb must stay terminator-free until
        // the very end: the legacy cached-literal helpers append their
        // initializing stores to it whenever a literal is first used.
        let prologue_bb = new llvm.BasicBlock("prologue", llvmFn);
        ir.setInsertPoint(prologue_bb);
        // link the frame before anything can allocate; slots
        // start as undefined so a pre-def walk sees valid ejsvals
        if (this.gc_frame)
            this.v.emitGCFrameLink(this.gc_frame, this.gc_frame_slots!.size, this.undef());
        const entry_params = eirFn.entry!.params;
        // params[0] = %env, params[1] = %this, rest are JS formals
        if (eirFn.sig) {
            // typed convention: formals arrive directly (raw doubles for
            // f64 formals) at llvm args [0..].  %env and %this have NO
            // argument slots and are required-unused — left unbound, so a
            // stray use fails loudly at val()
            for (let i = 2; i < entry_params.length; i++)
                this.values.set(entry_params[i]!, args[i - 2]!);
        } else {
            if (entry_params.length > 0) this.values.set(entry_params[0]!, env);
            if (entry_params.length > 1) {
                let this_val = ir.createLoad(types.EjsValue, this_ptr, "this");
                this.values.set(entry_params[1]!, this_val);
            }
            for (let i = 2; i < entry_params.length; i++)
                this.values.set(entry_params[i]!, this.emitArgLoad(argc, args_ptr, i - 2));
        }
        // demote slotted entry params (their canonical home is
        // the frame slot from here on; val() loads it per use)
        if (this.gc_frame_slots)
            for (const p of entry_params) this.demoteToSlot(p);
        // remember where the prologue ended; the branch into the eir entry
        // block is emitted *after* the body, because the legacy cached-
        // literal helpers append their initializing stores to the end of
        // whatever block is "entry" at the time they're first used.
        let prologue_end = ir.getInsertBlock();

        // emit every block's instructions
        for (let b of order) {
            ir.setInsertPoint(this.blocks.get(b)!);
            // slotted block params store to their frame slot at
            // block entry (after the phis, which the block-creation pass
            // already registered)
            if (this.gc_frame_slots && b !== eirFn.entry)
                for (const p of b.params) this.demoteToSlot(p);
            for (let inst of b.insts) {
                this.emitInst(inst);
                // a slotted def's store follows immediately
                // (slotted values are never targets-carrying, so the
                // block is not yet terminated here)
                if (this.gc_frame_slots && this.gc_frame_slots.has(inst))
                    this.demoteToSlot(inst);
            }
        }

        ir.setInsertPoint(prologue_end);
        ir.createBr(this.blocks.get(eirFn.entry!)!);
        ir.setInsertPoint(entry_bb);
        ir.createBr(prologue_bb);

        this.v.currentFunction = saved_function;
        return llvmFn;
    }

    // args[i] if i < argc, else undefined -- guarded load with a phi join
    emitArgLoad(argc: llvm.Value, args_ptr: llvm.Value, i: number): llvm.Value {
        let load_bb = new llvm.BasicBlock(`arg${i}_load`, this.llvmFn);
        let join_bb = new llvm.BasicBlock(`arg${i}_join`, this.llvmFn);
        const from_bb = ir.getInsertBlock()!;

        // materialize the fallback in the predecessor so it dominates the phi
        let undef_val = this.undef();
        let cmp = ir.createICmpUGt(argc, consts.int32(i), `has_arg${i}`);
        ir.createCondBr(cmp, load_bb, join_bb);

        ir.setInsertPoint(load_bb);
        let gep = ir.createGetElementPointer(types.EjsValue, args_ptr, [consts.int64(i)], "argp");
        let loaded = ir.createLoad(types.EjsValue, gep, `arg${i}`);
        ir.createBr(join_bb);

        ir.setInsertPoint(join_bb);
        let phi = ir.createPhi(types.EjsValue, 2, `arg${i}v`);
        phi.addIncoming(loaded, load_bb);
        phi.addIncoming(undef_val, from_bb);
        return phi;
    }

    emitCatchPrologue(eirBlock: Block): void {
        // landingpad; extract the exception; begin/end catch to fetch the
        // thrown ejsval.  end_catch releases the C++ exception object; the
        // value itself is safe (conservatively scanned like any other).
        let caught = ir.createLandingPad(types.EjsLandingPad, 1, "caught");
        caught.addClause(
            ir.createPointerCast(this.v.ejs_runtime.exception_typeinfo, types.Int8Pointer, "")
        );
        caught.setCleanup(true);
        if (!this.llvmFn.hasPersonality())
            this.llvmFn.setPersonality(
                ir.createPointerCast(this.v.ejs_runtime.personality, types.Int8Pointer, "personality")
            );

        let exc = ir.createExtractValue(caught, 0, "exc");
        let val = this.call(this.v.ejs_runtime.begin_catch, [exc], "caughtval");
        this.call(this.v.ejs_runtime.end_catch, [], "");

        const exc_param = eirBlock.params[0]!;
        this.values.set(exc_param, val);

        // the unwind discarded every callee frame below this one —
        // re-link our record as the chain head
        if (this.gc_frame) this.v.emitGCFrameRelink(this.gc_frame);
    }

    // if `v` has a frame slot, store its current llvm value there
    // (its canonical home; val() loads it per use from now on)
    demoteToSlot(v: Inst): void {
        const slot = this.gc_frame_slots ? this.gc_frame_slots.get(v) : undefined;
        if (slot === undefined) return;
        const cur = this.values.get(v);
        if (cur === undefined) return; // unbound (e.g. clone %env/%this)
        ir.createStore(cur, this.v.gcFrameSlotPtr(this.gc_frame!, slot));
    }

    maxOutgoingArgs(eirFn: Func): number {
        let max = 0;
        eirFn.forEachInst((inst) => {
            if (inst.op === "call") max = Math.max(max, inst.operands.length - 2);
            else if (inst.op === "construct" || inst.op === "construct_super")
                max = Math.max(max, inst.operands.length - 1);
            else if (inst.op === "construct_super_apply" || inst.op === "construct_apply")
                max = Math.max(max, 1);
            else if (inst.op === "make_array" || inst.op === "array_from_spread")
                max = Math.max(max, inst.operands.length);
            // names + values, spilled contiguously (see the emit case)
            else if (inst.op === "make_object_shaped")
                max = Math.max(max, inst.operands.length * 2);
            else if (inst.op === "fill_object_shaped")
                max = Math.max(max, (inst.operands.length - 1) * 2);
            else if (inst.op === "template_callsite")
                max = Math.max(
                    max,
                    (inst.imms["cooked"] as readonly string[]).length,
                    (inst.imms["raw"] as readonly string[]).length
                );
        });
        return max;
    }

    // --- helpers -------------------------------------------------------------------

    val(operand: Inst | null | undefined): llvm.Value {
        // a slotted value's canonical home is its gc-frame slot —
        // load per use, so a post-safepoint use observes any collector
        // rewrite.  Loads between safepoints CSE under LLVM; loads
        // across one cannot (the frame escapes via the chain).
        if (operand && this.gc_frame_slots) {
            const slot = this.gc_frame_slots.get(operand);
            if (slot !== undefined)
                return ir.createLoad(
                    types.EjsValue,
                    this.v.gcFrameSlotPtr(this.gc_frame!, slot),
                    `gcf_v${operand.id}`
                );
        }
        const v = operand ? this.values.get(operand) : undefined;
        if (v === undefined)
            throw new Error(
                `EIR emit: no llvm value for %v${operand ? operand.id : "<null>"} (${operand ? operand.op : "?"})`
            );
        return v;
    }

    undef(): llvm.Value {
        return this.v.loadUndefinedEjsValue();
    }

    call(callee: llvm.EjsFunction, argv: llvm.Value[], name?: string): llvm.Value {
        return this.abi.createCall(this.llvmFn, callee.type, callee, argv, name || "");
    }

    // the emitted generational write barrier (object-
    // remembering).  Inline: one range check on the stored VALUE; slow:
    // _ejs_gc_remember_val(owner, value) marks the owner dirty.  With
    // the nursery disabled the bounds are zero and the branch is never
    // taken.
    emitStoreBarrier(owner: llvm.Value, v: llvm.Value): void {
        const rt = this.v.ejs_runtime;
        const young = this.v.emitYoungCheck(v);
        const bar_bb = new llvm.BasicBlock("wb_slow", this.llvmFn);
        const cont_bb = new llvm.BasicBlock("wb_cont", this.llvmFn);
        ir.createCondBr(young, bar_bb, cont_bb);
        ir.setInsertPoint(bar_bb);
        this.call(rt.gc_write_barrier, [owner, v]);
        ir.createBr(cont_bb);
        ir.setInsertPoint(cont_bb);
    }

    // THE slot-addressing seam.  A shaped object's
    // property storage is a closureenv slot array hanging off the
    // map/slots union word; when the GC work moves slots inline,
    // only this method changes (the ops carry slot indices, not
    // addresses).  Only valid downstream of a passed has_shape on `objval`
    // for a shape with more than `slot` fields — which the EIR verifier
    // enforces — so the union word is a non-null slot-array ejsval here.
    slotRef(objval: llvm.Value, slot: number): llvm.Value {
        const objptr = this.v.objectPointer(objval);
        // field 4 of types.EjsObject is the map/slots union word; load it
        // as an ejsval (the slot-array reference)
        const union_ptr = ir.createInBoundsGetElementPointer(
            types.EjsObject,
            objptr,
            [consts.int64(0), consts.int32(4)],
            "slots_union_ptr"
        );
        const slots_ptr = ir.createBitCast(
            union_ptr,
            types.EjsValue.pointerTo(),
            "slots_ejsval_ptr"
        );
        const slotsval = ir.createLoad(types.EjsValue, slots_ptr, "slots_ejsval");
        // payload-mask the closureenv ejsval to its EJSClosureEnv*
        const envptr = ir.createPointerCast(
            this.v.objectPointer(slotsval),
            types.EjsClosureEnv.pointerTo(),
            "slots_env"
        );
        // field 4 of types.EjsClosureEnv is the trailing slots array; the
        // GEP is deliberately non-inbounds (the array is declared [1 x
        // ejsval], the moduleSlotRef precedent for trailing arrays)
        return ir.createGetElementPointer(
            types.EjsClosureEnv,
            envptr,
            [consts.int64(0), consts.int32(4), consts.int64(slot)],
            "slot_ref"
        );
    }

    // same shape as the legacy opencoded module slot access: a non-inbounds
    // GEP into the module global (see handleModuleSlotRef in compiler.js).
    // "%self" refers to the module being compiled.
    moduleSlotRef(moduleString: string, slot: number): llvm.Value {
        let module_global;
        if (moduleString === "%self") module_global = this.v.this_module_global;
        else module_global = this.v.import_module_globals.get(moduleString);
        if (!module_global)
            throw new Error(`EIR emit: no module global for '${moduleString}'`);
        let mg = ir.createPointerCast(module_global, types.EjsModule.pointerTo(), "");
        return ir.createGetElementPointer(
            types.EjsModule,
            mg,
            [consts.int64(0), consts.int32(3), consts.int64(slot)],
            "slot_ref"
        );
    }

    // spill values into the scratch area, returning an EjsValue* to its start
    spillArgs(values: llvm.Value[]): llvm.Value {
        for (let i = 0; i < values.length; i++) {
            const gep = ir.createGetElementPointer(
                this.scratch_type!,
                this.scratch!,
                [consts.int32(0), consts.int64(i)],
                `sp${i}`
            );
            ir.createStore(values[i]!, gep);
        }
        return ir.createGetElementPointer(
            this.scratch_type!,
            this.scratch!,
            [consts.int32(0), consts.int64(0)],
            "spargs"
        );
    }

    // emit a call to `callee` that respects this instruction's normal/unwind
    // targets (invoke) or is a plain call
    emitCallLike(inst: Inst, callee: llvm.EjsFunction, argv: llvm.Value[], name?: string): llvm.Value {
        if (inst.targets && inst.targets.length > 0) {
            let normal = null;
            let unwind = null;
            for (let t of inst.targets) {
                if (t.kind === "unwind") unwind = t;
                else normal = t;
            }
            this.addEdgeIncomings(inst, unwind);
            this.addEdgeIncomings(inst, normal);
            const normal_bb = this.blocks.get(normal!.block)!;
            const unwind_bb = this.blocks.get(unwind!.block)!;
            let rv = this.abi.createInvoke(
                this.llvmFn,
                callee.type,
                callee,
                argv,
                normal_bb,
                unwind_bb,
                name || ""
            );
            this.values.set(inst, rv);
            return rv;
        }
        let rv = this.call(callee, argv, name);
        this.values.set(inst, rv);
        return rv;
    }

    // fill in phi incomings for the arguments this edge passes
    addEdgeIncomings(inst: Inst, target: Target | null | undefined): void {
        if (!target) return;
        const src_bb = ir.getInsertBlock()!;
        let params = target.block.params;
        let arg_base = target.block.isCatch ? 1 : 0;
        for (let i = 0; i < target.args.length; i++) {
            const param = params[arg_base + i]!;
            const phi = this.phis.get(param);
            if (!phi) throw new Error("EIR emit: edge argument for missing phi");
            phi.addIncoming(this.val(target.args[i]), src_bb);
        }
    }

    // --- instruction emission -----------------------------------------------------------

    emitInst(inst: Inst): llvm.Value | void {
        let rt = this.v.ejs_runtime;

        switch (inst.op) {
            case "const": {
                let v;
                switch ((inst.imms["kind"] as string)) {
                    case "number":
                        v = this.v.loadDoubleEjsValue(inst.imms["value"] as number);
                        break;
                    case "atom":
                        v = this.v.getAtom(String(inst.imms.value));
                        break;
                    case "boolean":
                        v = this.v.loadBoolEjsValue(inst.imms["value"] as boolean);
                        break;
                    case "undefined":
                        v = this.undef();
                        break;
                    case "null":
                        v = this.v.loadNullEjsValue();
                        break;
                    default:
                        throw new Error(`EIR emit: const kind ${(inst.imms["kind"] as string)}`);
                }
                this.values.set(inst, v);
                return;
            }

            case "to_boolean": {
                // produce an i1 for cond_br; only ever consumed by cond_br
                let truthy = this.call(rt.truthy, [this.val(inst.operands[0])], "truthy");
                let b = ir.createICmpEq(truthy, consts.True(), "tobool");
                this.values.set(inst, b);
                return;
            }

            case "typeof_is": {
                // the single-tag test cleanup.ts rewrites
                // `typeof x === "T"` into; boxed boolean result via the
                // runtime's typeof_is_<type> entries
                const t = String(inst.imms["type"]);
                const callee = (rt as unknown as Record<string, import("@llvm").EjsFunction | undefined>)[
                    `typeof_is_${t}`
                ];
                if (!callee) throw new Error(`EIR emit: no typeof_is runtime entry for '${t}'`);
                return this.emitCallLike(inst, callee, [this.val(inst.operands[0])], "typeofis");
            }

            // --- the typed low tier ---------------------------
            // has_tag/unbox/box mirror LLVMIRVisitor's NaN-boxing helpers;
            // the f64_* ops are plain LLVM float arithmetic.  has_tag and
            // f64_lt produce machine i1 (consumed by cond_br, like
            // to_boolean); unbox produces a raw double; box re-enters the
            // boxed world.
            case "has_tag": {
                const tag = inst.imms["tag"];
                if (tag !== "number")
                    throw new Error(`EIR emit: has_tag tag '${String(tag)}' is not supported`);
                this.values.set(inst, this.v.isNumber(this.val(inst.operands[0])));
                return;
            }

            // --- shapes -------------------------------
            // has_shape folds the NaN-box object check into the header
            // shape-index compare, the way isNumber backs has_tag: a
            // non-object is simply false.  The shape-index global holds
            // EJS_SHAPE_NOMATCH until module init interns the real index
            // (and forever, under EJS_SHAPES=off) — an index no object
            // header can carry, so the guard is false rather than wrong.
            case "has_shape": {
                const key = String(inst.imms["shape"]);
                const fields = this.eirModule.shapes.get(key);
                if (!fields)
                    throw new Error(`EIR emit: has_shape names unknown module shape '${key}'`);
                const g = this.v.moduleShapeGlobal(key, fields);
                const val = this.val(inst.operands[0]);

                const check_bb = new llvm.BasicBlock("shape_check", this.llvmFn);
                const merge_bb = new llvm.BasicBlock("shape_merge", this.llvmFn);
                const from_bb = ir.getInsertBlock()!;
                ir.createCondBr(this.v.isObject(val), check_bb, merge_bb);

                ir.setInsertPoint(check_bb);
                const objptr = this.v.objectPointer(val);
                // GCObjectHeader is two i32 halves in types.EjsObject; the
                // shape index is the low 24 bits of the high half
                const hdr_hi_ptr = ir.createInBoundsGetElementPointer(
                    types.EjsObject,
                    objptr,
                    [consts.int64(0), consts.int32(1)],
                    "hdr_hi_ptr"
                );
                const hdr_hi = ir.createLoad(types.Int32, hdr_hi_ptr, "hdr_hi");
                const shape_idx = ir.createAnd(hdr_hi, consts.int32(0xffffff), "shape_idx");
                const want = ir.createLoad(types.Int32, g, "shape_want");
                const eq = ir.createICmpEq(shape_idx, want, "shape_eq");
                ir.createBr(merge_bb);

                ir.setInsertPoint(merge_bb);
                const phi = ir.createPhi(types.Int1, 2, "has_shape");
                phi.addIncoming(eq, check_bb);
                phi.addIncoming(consts.int1(0), from_bb);
                this.values.set(inst, phi);
                return;
            }
            // typed slots: an f64-repr slot is accessed as a raw
            // double — same address, same 8 bytes (the NaN-box stores
            // doubles raw), just loaded/stored as the machine type the
            // guard's repr proof licenses.
            case "slot_load": {
                const ref = this.slotRef(this.val(inst.operands[0]), inst.imms["slot"] as number);
                if (inst.imms["repr"] === "f64") {
                    const dref = ir.createBitCast(ref, types.Double.pointerTo(), "slot_f64_ptr");
                    this.values.set(inst, ir.createLoad(types.Double, dref, "slot_f64"));
                } else {
                    this.values.set(inst, ir.createLoad(types.EjsValue, ref, "slot_val"));
                }
                return;
            }
            case "slot_store": {
                const objval = this.val(inst.operands[0]);
                const ref = this.slotRef(objval, inst.imms["slot"] as number);
                if (inst.imms["repr"] === "f64") {
                    // raw doubles are not references: no barrier
                    const dref = ir.createBitCast(ref, types.Double.pointerTo(), "slot_f64_ptr");
                    ir.createStore(this.val(inst.operands[1]), dref);
                } else {
                    ir.createStore(this.val(inst.operands[1]), ref);
                    // the barrier owner is the wrapper OBJECT:
                    // its Scan walks the slot values directly, and
                    // embedded storage is not a cell of its own
                    this.emitStoreBarrier(objval, this.val(inst.operands[1]));
                }
                this.values.set(inst, this.val(inst.operands[1]));
                return;
            }
            // born with their shape: spill the field
            // names (atom loads) and initial values contiguously into the
            // scratch area — names at [0..n), values at [n..2n) — and make
            // one runtime call.  The runtime re-derives the true shape from
            // the actual values and falls back to sequential generic sets
            // whenever the shaped fast path doesn't apply, so no shape
            // global is consulted here (unlike has_shape).
            case "make_object_shaped":
            case "fill_object_shaped": {
                const key = String(inst.imms["shape"]);
                const fields = this.eirModule.shapes.get(key);
                if (!fields)
                    throw new Error(`EIR emit: ${inst.op} names unknown module shape '${key}'`);
                const isFill = inst.op === "fill_object_shaped";
                const vals = inst.operands.slice(isFill ? 1 : 0).map((o) => this.val(o));
                const names = fields.map((f) => this.v.getAtom(f.name));
                const base = this.spillArgs([...names, ...vals]);
                const vbase = ir.createGetElementPointer(
                    this.scratch_type!,
                    this.scratch!,
                    [consts.int32(0), consts.int64(fields.length)],
                    "shaped_vals"
                );
                const argv = isFill
                    ? [this.val(inst.operands[0]), consts.int32(fields.length), base, vbase]
                    : [consts.int32(fields.length), base, vbase];
                this.emitCallLike(
                    inst,
                    isFill ? rt.object_fill_shaped : rt.object_new_shaped,
                    argv,
                    isFill ? "fillshaped" : "newshaped"
                );
                return;
            }
            case "epoch_check":
                this.values.set(inst, this.v.emitAccessorEpochCheck());
                return;
            case "unbox_f64":
                this.values.set(inst, this.v.unboxDouble(this.val(inst.operands[0])));
                return;
            case "f64_const":
                this.values.set(inst, llvm.ConstantFP.getDouble(inst.imms["value"] as number));
                return;
            case "box_f64":
                this.values.set(inst, this.v.boxDouble(this.val(inst.operands[0])));
                return;
            case "f64_add":
                this.values.set(
                    inst,
                    ir.createFAdd(this.val(inst.operands[0]), this.val(inst.operands[1]), "f64_add")
                );
                return;
            case "f64_sub":
                this.values.set(
                    inst,
                    ir.createFSub(this.val(inst.operands[0]), this.val(inst.operands[1]), "f64_sub")
                );
                return;
            case "f64_mul":
                this.values.set(
                    inst,
                    ir.createFMul(this.val(inst.operands[0]), this.val(inst.operands[1]), "f64_mul")
                );
                return;
            case "f64_div":
                this.values.set(
                    inst,
                    ir.createFDiv(this.val(inst.operands[0]), this.val(inst.operands[1]), "f64_div")
                );
                return;
            case "f64_lt":
                this.values.set(
                    inst,
                    ir.createFCmpOLT(this.val(inst.operands[0]), this.val(inst.operands[1]), "f64_lt")
                );
                return;

            case "get_prop": {
                let callee = rt.object_getprop;
                return this.emitCallLike(
                    inst,
                    callee,
                    [this.val(inst.operands[0]), this.val(inst.operands[1])],
                    "getprop"
                );
            }
            case "get_prop_atom": {
                let key = this.v.getAtom(String(inst.imms["atom"]));
                return this.emitCallLike(
                    inst,
                    rt.object_getprop,
                    [this.val(inst.operands[0]), key],
                    "getprop"
                );
            }
            case "set_prop": {
                return this.emitCallLike(
                    inst,
                    rt.object_setprop,
                    [
                        this.val(inst.operands[0]),
                        this.val(inst.operands[1]),
                        this.val(inst.operands[2]),
                    ],
                    "setprop"
                );
            }
            case "set_prop_atom": {
                let key = this.v.getAtom(String(inst.imms["atom"]));
                return this.emitCallLike(
                    inst,
                    rt.object_setprop,
                    [this.val(inst.operands[0]), key, this.val(inst.operands[1])],
                    "setprop"
                );
            }

            case "delete_prop": {
                return this.emitCallLike(
                    inst,
                    rt.unopdelete,
                    [this.val(inst.operands[0]), this.val(inst.operands[1])],
                    "delres"
                );
            }

            case "module_get_exotic": {
                // the module object itself as an ejsval (namespace imports).
                // JS modules have a link-time global (matches the opencoded
                // legacy handleModuleGetExotic); native modules only exist
                // at runtime, resolved by name through module_get.
                const moduleString = String(inst.imms["module"]);
                let module_global: import("@llvm").GlobalVariable | undefined;
                if (moduleString === "%self") module_global = this.v.this_module_global;
                else module_global = this.v.import_module_globals.get(moduleString);
                if (module_global) {
                    let rv = this.v.emitEjsvalFromPtr(module_global, "exotic");
                    this.values.set(inst, rv);
                    return rv;
                }
                let name = this.v.getAtom(String(moduleString));
                return this.emitCallLike(inst, rt.module_get, [name], "exotic");
            }

            case "module_slot_load": {
                const slot_ref = this.moduleSlotRef(String(inst.imms["module"]), inst.imms["slot"] as number);
                this.values.set(inst, ir.createLoad(types.EjsValue, slot_ref, "module_slot"));
                return;
            }
            case "module_slot_store": {
                const slot_ref = this.moduleSlotRef(String(inst.imms["module"]), inst.imms["slot"] as number);
                ir.createStore(this.val(inst.operands[0]), slot_ref);
                this.values.set(inst, this.val(inst.operands[0]));
                return;
            }

            case "get_global": {
                let key = this.v.getAtom(String(inst.imms["atom"]));
                return this.emitCallLike(inst, rt.global_getprop, [key], "getglobal");
            }
            case "set_global": {
                let key = this.v.getAtom(String(inst.imms["atom"]));
                return this.emitCallLike(
                    inst,
                    rt.global_setprop,
                    [key, this.val(inst.operands[0])],
                    "setglobal"
                );
            }

            case "make_env": {
                const n = inst.imms["size"] as number;
                // envs dominate allocation counts — bump-allocate
                // inline; the runtime call is the slow path/safepoint.
                // -fno-inline-alloc is the compile-time bisect hook.
                const slow = () => this.call(rt.make_closure_env, [consts.int32(n)], "env");
                const rv = passes().inlineAlloc ? this.v.emitEnvAllocInline(n, slow) : slow();
                this.values.set(inst, rv);
                return;
            }
            case "env_load": {
                // inline slot addressing, recomputed per use
                // from the boxed env (a relocated env re-derives) —
                // deletes a runtime call per access.  -fno-inline-env-slots
                // restores the runtime-call path.
                let ref = passes().inlineEnvSlots
                    ? this.v.emitEnvSlotRef(
                          this.val(inst.operands[0]),
                          inst.imms["slot"] as number
                      )
                    : this.call(
                          rt.get_env_slot_ref,
                          [this.val(inst.operands[0]), consts.int32((inst.imms["slot"] as number))],
                          "slotref"
                      );
                this.values.set(inst, ir.createLoad(types.EjsValue, ref, "slot"));
                return;
            }
            case "env_store": {
                let ref = passes().inlineEnvSlots
                    ? this.v.emitEnvSlotRef(
                          this.val(inst.operands[0]),
                          inst.imms["slot"] as number
                      )
                    : this.call(
                          rt.get_env_slot_ref,
                          [this.val(inst.operands[0]), consts.int32((inst.imms["slot"] as number))],
                          "slotref"
                      );
                ir.createStore(this.val(inst.operands[1]), ref);
                this.emitStoreBarrier(this.val(inst.operands[0]), this.val(inst.operands[1]));
                this.values.set(inst, this.val(inst.operands[1]));
                return;
            }
            case "make_closure": {
                let target = this.llvm_fns.get((inst.imms["fn"] as string));
                if (!target) throw new Error(`EIR emit: unknown closure target ${(inst.imms["fn"] as string)}`);
                let name = this.v.getAtom(
                    String(inst.imms.name !== undefined ? inst.imms.name : (inst.imms["fn"] as string))
                );
                let rv = this.call(
                    rt.make_closure,
                    [
                        this.val(inst.operands[0]),
                        name,
                        target,
                        consts.int32((inst.imms["len"] as number) ?? 0),
                    ],
                    "closure"
                );
                this.values.set(inst, rv);
                return;
            }

            case "call": {
                const direct = inst.imms["direct"] as string | undefined;
                if (direct) {
                    const target = this.llvm_fns.get(direct);
                    if (!target)
                        throw new Error(`EIR emit: unknown direct callee ${direct}`);
                    let env_val = this.val(inst.operands[0]);
                    let this_val = this.val(inst.operands[1]);
                    let dargs = inst.operands.slice(2).map((o) => this.val(o));
                    ir.createStore(this_val, this.this_slot);
                    let dargv;
                    if (dargs.length > 0) dargv = this.spillArgs(dargs);
                    else dargv = ir.createPointerCast(this.this_slot, types.EjsValue.pointerTo(), "noargs");
                    return this.emitCallLike(
                        inst,
                        target,
                        [env_val, this.this_slot, consts.int32(dargs.length), dargv, this.undef()],
                        "dcall"
                    );
                }
                let callee = this.val(inst.operands[0]);
                let this_val = this.val(inst.operands[1]);
                let args = inst.operands.slice(2).map((o) => this.val(o));
                ir.createStore(this_val, this.this_slot);
                let argv;
                if (args.length > 0) argv = this.spillArgs(args);
                else argv = ir.createPointerCast(this.this_slot, types.EjsValue.pointerTo(), "noargs");
                return this.emitCallLike(
                    inst,
                    rt.invoke_closure,
                    [callee, this.this_slot, consts.int32(args.length), argv, this.undef()],
                    "callres"
                );
            }
            case "call_typed": {
                // direct call to a specialized clone — args are
                // raw machine values in registers, no scratch spill, no
                // closure dispatch.  Operand 0 (the EIR-level env slot) is
                // NOT passed: clone signatures carry the formals alone
                // (env is required-unused by the specialization checks)
                const target = this.llvm_fns.get(inst.imms["fn"] as string);
                if (!target)
                    throw new Error(`EIR emit: unknown call_typed callee ${String(inst.imms["fn"])}`);
                const argv = inst.operands.slice(1).map((o) => this.val(o));
                return this.emitCallLike(inst, target, argv, "tcall");
            }
            case "construct": {
                let callee = this.val(inst.operands[0]);
                let args = inst.operands.slice(1).map((o) => this.val(o));
                ir.createStore(this.undef(), this.this_slot);
                let argv;
                if (args.length > 0) argv = this.spillArgs(args);
                else argv = ir.createPointerCast(this.this_slot, types.EjsValue.pointerTo(), "noargs");
                return this.emitCallLike(
                    inst,
                    rt.construct_closure,
                    [callee, this.this_slot, consts.int32(args.length), argv, callee],
                    "ctorres"
                );
            }
            case "construct_super": {
                // the super constructor writes the constructed object back
                // through OUR incoming &this (that's how a derived ctor's
                // result reaches the runtime's construct machinery), and
                // newTarget passes through unchanged
                let callee = this.val(inst.operands[0]);
                let args = inst.operands.slice(1).map((o) => this.val(o));
                let argv;
                if (args.length > 0) argv = this.spillArgs(args);
                else argv = ir.createPointerCast(this.this_slot, types.EjsValue.pointerTo(), "noargs");
                return this.emitCallLike(
                    inst,
                    rt.construct_closure,
                    [callee, this.fn_this_ptr, consts.int32(args.length), argv, this.fn_new_target],
                    "csuper"
                );
            }
            case "construct_super_apply": {
                // operands = [super_ctor, args_array]; the runtime asserts
                // argc == 1 and spreads the dense array itself
                let callee = this.val(inst.operands[0]);
                let argv = this.spillArgs([this.val(inst.operands[1])]);
                return this.emitCallLike(
                    inst,
                    rt.construct_closure_apply,
                    [callee, this.fn_this_ptr, consts.int32(1), argv, this.fn_new_target],
                    "csuperapply"
                );
            }
            case "construct_apply": {
                // like construct, but the args arrive as one dense array
                // the runtime spreads; this_slot supplies the out-param
                let callee = this.val(inst.operands[0]);
                let argv = this.spillArgs([this.val(inst.operands[1])]);
                ir.createStore(this.undef(), this.this_slot);
                return this.emitCallLike(
                    inst,
                    rt.construct_closure_apply,
                    [callee, this.this_slot, consts.int32(1), argv, callee],
                    "ctorapply"
                );
            }
            case "new_target": {
                this.values.set(inst, this.fn_new_target);
                return;
            }

            case "make_array": {
                let elems = inst.operands.map((o) => this.val(o));
                if ((inst.imms["indices"] as readonly number[]) === undefined) {
                    let argv;
                    if (elems.length > 0) argv = this.spillArgs(elems);
                    else argv = ir.createPointerCast(this.this_slot, types.EjsValue.pointerTo(), "noargs");
                    return this.emitCallLike(
                        inst,
                        rt.array_new_copy,
                        [consts.int64(elems.length), argv],
                        "arr"
                    );
                }
                // an array literal with holes: force-filled allocation plus
                // per-index stores for the non-holes (matching the legacy
                // visitArrayExpression)
                let arr = this.call(
                    rt.array_new,
                    [consts.int64((inst.imms["len"] as number)), consts.bool(true)],
                    "arr"
                );
                this.values.set(inst, arr);
                for (let i = 0; i < elems.length; i++) {
                    const key = this.v.loadDoubleEjsValue((inst.imms["indices"] as readonly number[])[i]!);
                    this.call(rt.object_setprop, [arr, key, elems[i]!], "");
                }
                return arr;
            }
            case "define_accessor_computed": {
                // a partial accessor descriptor: only the getter or only
                // the setter is present; the runtime merges into any
                // existing accessor property.  enumerable+configurable,
                // like the atom-keyed case.
                let isGet = (inst.imms["kind"] as string) === "get";
                let flags = 0x33 | (isGet ? 0x100 : 0x200);
                let accessor = this.val(inst.operands[2]);
                let undef = this.v.loadUndefinedEjsValue();
                return this.emitCallLike(
                    inst,
                    rt.object_define_accessor_prop_desc,
                    [
                        this.val(inst.operands[0]),
                        this.val(inst.operands[1]),
                        isGet ? accessor : undef,
                        isGet ? undef : accessor,
                        consts.int32(flags),
                    ],
                    "define_accessor_computed"
                );
            }
            case "define_accessor": {
                // flags 0x19 = enumerable | configurable, matching the
                // legacy visitObjectExpression
                let key = this.v.getAtom(String(inst.imms["atom"]));
                return this.emitCallLike(
                    inst,
                    rt.object_define_accessor_prop,
                    [
                        this.val(inst.operands[0]),
                        key,
                        this.val(inst.operands[1]),
                        this.val(inst.operands[2]),
                        consts.int32(0x19),
                    ],
                    "defaccessor"
                );
            }
            case "array_from_spread": {
                // concatenate the operands (array chunks / iterables) into
                // a fresh array, like the legacy handleArrayFromSpread
                let elems = inst.operands.map((o) => this.val(o));
                let argv;
                if (elems.length > 0) argv = this.spillArgs(elems);
                else argv = ir.createPointerCast(this.this_slot, types.EjsValue.pointerTo(), "noargs");
                return this.emitCallLike(
                    inst,
                    rt.array_from_iterables,
                    [consts.int32(elems.length), argv],
                    "spreadarr"
                );
            }
            case "make_object": {
                let proto = ir.createLoad(
                    types.EjsValue,
                    this.v.ejs_globals["Object_prototype"]!,
                    "objproto"
                );
                let obj = this.call(rt.object_create, [proto], "obj");
                this.values.set(inst, obj);
                for (let i = 0; i < inst.operands.length; i++) {
                    let key = this.v.getAtom(String((inst.imms["keys"] as readonly string[])[i]));
                    this.call(rt.object_setprop, [obj, key, this.val(inst.operands[i])], "");
                }
                return;
            }

            // --- control flow ---------------------------------------------------

            case "br": {
                const t = inst.targets![0]!;
                this.addEdgeIncomings(inst, t);
                ir.createBr(this.blocks.get(t.block)!);
                return;
            }
            case "cond_br": {
                let cond = this.val(inst.operands[0]);
                // prop_iter_next produces the runtime's i8 EJSBool; every
                // other condition source (to_boolean) is already an i1
                if (inst.operands[0]!.op === "prop_iter_next")
                    cond = ir.createICmpEq(cond, consts.True(), "moreleft_i1");
                this.addEdgeIncomings(inst, inst.targets![0]);
                this.addEdgeIncomings(inst, inst.targets![1]);
                ir.createCondBr(
                    cond,
                    this.blocks.get(inst.targets![0]!.block)!,
                    this.blocks.get(inst.targets![1]!.block)!
                );
                return;
            }
            case "return": {
                // the return value is read BEFORE the unlink (it
                // may itself load from a frame slot); then pop the frame
                const rv = this.val(inst.operands[0]);
                if (this.gc_frame) this.v.emitGCFrameUnlink(this.gc_frame);
                // an f64-result clone returns the raw double directly (a
                // plain scalar return needs none of the ABI's ejsval
                // struct-return handling)
                if (this.eirFn.sig && this.eirFn.sig.result === "f64") ir.createRet(rv);
                else this.abi.createRet(this.llvmFn, rv);
                return;
            }
            case "throw": {
                let throw_fn = this.v.ejs_runtime.throw;
                if (inst.targets && inst.targets.length > 0) {
                    // unwinds to a local handler
                    let unwind = inst.targets[0];
                    this.addEdgeIncomings(inst, unwind);
                    let cont = new llvm.BasicBlock("throw_unreachable", this.llvmFn);
                    this.abi.createInvoke(
                        this.llvmFn,
                        throw_fn.type,
                        throw_fn,
                        [this.val(inst.operands[0])],
                        cont,
                        this.blocks.get(unwind!.block)!,
                        ""
                    );
                    ir.setInsertPoint(cont);
                    ir.createUnreachable();
                } else {
                    this.call(throw_fn, [this.val(inst.operands[0])], "");
                    ir.createUnreachable();
                }
                return;
            }
            case "unreachable": {
                ir.createUnreachable();
                return;
            }

            case "template_callsite": {
                // mirror the legacy handleTemplateCallsite: a zeroinit
                // per-site global, built lazily (a zeroed ejsval reads as
                // number 0.0 — the is-number check doubles as
                // "uninitialized"), arrays frozen, cooked.raw = raw
                let cooked_strs = (inst.imms["cooked"] as readonly string[]);
                let raw_strs = (inst.imms["raw"] as readonly string[]);
                let g = new llvm.GlobalVariable(
                    this.module,
                    types.EjsValue,
                    `_ejs_eir_callsite_${mangle_gen++}`,
                    llvm.Constant.getAggregateZero(types.EjsValue),
                    false
                );
                let loaded = this.v.createEjsValueLoad(g, "callsite_load");
                let then_bb = new llvm.BasicBlock("callsite_build", this.llvmFn);
                let merge_bb = new llvm.BasicBlock("callsite_merge", this.llvmFn);
                const from_bb = ir.getInsertBlock()!;
                let isnum = this.v.isNumber(loaded);
                ir.createCondBr(isnum, then_bb, merge_bb);

                ir.setInsertPoint(then_bb);
                this.call(rt.gc_add_root, [g], "");
                const mkarr = (strs: readonly string[], name: string) => {
                    const vals = strs.map((s) => this.v.getAtom(String(s)));
                    let argv: import("@llvm").Value;
                    if (vals.length > 0) argv = this.spillArgs(vals);
                    else argv = ir.createPointerCast(this.this_slot, types.EjsValue.pointerTo(), "noargs");
                    return this.call(rt.array_new_copy, [consts.int64(vals.length), argv], name);
                };
                let cooked = mkarr(cooked_strs, "callsite_cooked");
                let raw = mkarr(raw_strs, "callsite_raw");
                let frozen_raw = this.call(rt.object_freeze, [raw], "frozen_raw");
                this.call(rt.object_setprop, [cooked, this.v.getAtom("raw"), frozen_raw], "");
                let frozen = this.call(rt.object_freeze, [cooked], "frozen_cooked");
                ir.createStore(frozen, g);
                const built_bb = ir.getInsertBlock()!;
                ir.createBr(merge_bb);

                ir.setInsertPoint(merge_bb);
                let phi = ir.createPhi(types.EjsValue, 2, "callsite");
                phi.addIncoming(loaded, from_bb);
                phi.addIncoming(frozen, built_bb);
                this.values.set(inst, phi);
                return;
            }
            case "make_regexp": {
                let source = consts.string(ir, (inst.imms["source"] as string));
                let flags = consts.string(ir, (inst.imms["flags"] as string));
                return this.emitCallLike(inst, rt.regexp_new_utf8, [source, flags], "regexp");
            }

            case "rest_args": {
                // rest = argc > index ? array_new_copy(argc - index, args + index)
                //                     : array_new_copy(0, args)
                // (count of zero never dereferences the pointer, so the
                // select keeps this branch-free)
                let index = (inst.imms["index"] as number);
                let has_rest = ir.createICmpSGt(this.fn_argc, consts.int32(index), "has_rest");
                let count = ir.createNswSub(this.fn_argc, consts.int32(index), "rest_count");
                count = ir.createSelect(has_rest, count, consts.int32(0), "rest_count_sel");
                count = ir.createZExt(count, types.Int64, "rest_count64");
                let gep = ir.createGetElementPointer(
                    types.EjsValue,
                    this.fn_args_ptr,
                    [consts.int64(index)],
                    "rest_args"
                );
                let ptr = ir.createSelect(has_rest, gep, this.fn_args_ptr, "rest_ptr");
                let rv = this.call(rt.array_new_copy, [count, ptr], "rest");
                this.values.set(inst, rv);
                return rv;
            }

            case "args_obj": {
                return this.emitCallLike(
                    inst,
                    rt.arguments_new,
                    [this.fn_argc, this.fn_args_ptr],
                    "argsobj"
                );
            }

            case "arg_len": {
                // max(argc - index, 0) boxed, computed by a pure runtime
                // helper (the argc register is the only input — no argv
                // read, no allocation)
                const index = (inst.imms["index"] as number) || 0;
                const rv = this.call(rt.arg_length, [this.fn_argc, consts.int32(index)], "arg_len");
                this.values.set(inst, rv);
                return rv;
            }

            case "prop_iter_new": {
                return this.emitCallLike(
                    inst,
                    rt.prop_iterator_new,
                    [this.val(inst.operands[0])],
                    "propiter"
                );
            }
            case "prop_iter_next": {
                // returns the runtime's i1 directly; consumed by cond_br
                return this.emitCallLike(
                    inst,
                    rt.prop_iterator_next,
                    [this.val(inst.operands[0]), consts.True()],
                    "moreleft"
                );
            }
            case "prop_iter_current": {
                return this.emitCallLike(
                    inst,
                    rt.prop_iterator_current,
                    [this.val(inst.operands[0])],
                    "propcur"
                );
            }

            case "call_runtime": {
                // a direct call to a named entry in the runtime method table
                const rtName = String(inst.imms["name"]);
                const callee = (rt as unknown as Record<string, import("@llvm").EjsFunction | undefined>)[rtName];
                if (!callee) throw new Error(`EIR emit: no runtime function '${rtName}'`);
                let argv = inst.operands.map((o) => this.val(o));
                if ((inst.imms["void"] as boolean | undefined)) {
                    // void results can't be named (LLVM) or read as values.
                    // materialize the placeholder BEFORE the call: an
                    // invoke (in a protected region) terminates the block.
                    let undef_val = this.undef();
                    this.emitCallLike(inst, callee, argv, "");
                    this.values.set(inst, undef_val);
                    return;
                }
                return this.emitCallLike(inst, callee, argv, "rtres");
            }

            case "to_numeric":
                return this.emitCallLike(inst, rt.op_to_numeric, [this.val(inst.operands[0])], "tonum");

            default: {
                // generic binops / unops through the runtime interfaces
                let binop = binop_for_op[inst.op];
                if (binop) {
                    // ++/-- adds carry the update imm: the increment-
                    // flavored entries keep BigInt in-type instead of
                    // throwing the mixed-operand TypeError
                    let callee =
                        inst.imms["update"] && inst.op === "add" ? rt.op_add_update :
                        inst.imms["update"] && inst.op === "sub" ? rt.op_sub_update :
                        this.v.ejs_binops[binop];
                    if (!callee) throw new Error(`EIR emit: no binop interface for ${binop}`);
                    return this.emitCallLike(
                        inst,
                        callee,
                        [this.val(inst.operands[0]), this.val(inst.operands[1])],
                        "binres"
                    );
                }
                const unop = unop_for_op[inst.op];
                if (unop) {
                    const callee = (this.v.ejs_runtime as unknown as Record<string, import("@llvm").EjsFunction | undefined>)[`unop${unop}`];
                    if (!callee) throw new Error(`EIR emit: no unop interface for ${unop}`);
                    return this.emitCallLike(inst, callee, [this.val(inst.operands[0])], "unres");
                }
                throw new Error(`EIR emit: unhandled opcode '${inst.op}'`);
            }
        }
    }
}
