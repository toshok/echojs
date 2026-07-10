/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
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
// module, abi, runtime interface, atom/string-literal machinery), so EIR
// functions and legacy functions coexist in one compilation unit.  The
// legacy path calls into EIR functions through a small forwarding thunk
// (see compiler.js visitFunction), which keeps closure creation and env
// plumbing entirely on the legacy side for now.

import * as llvm from "@llvm";
import * as types from "../types";
import * as consts from "../consts";

let ir = llvm.IRBuilder;

// EIR opcode -> the operator key used by runtime.js's binop interface
const binop_for_op = {
    add: "+",
    sub: "-",
    mul: "*",
    div: "/",
    mod: "%",
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

const unop_for_op = {
    logical_not: "!",
    neg: "-",
    unary_plus: "+",
    bitnot: "~",
    typeof: "typeof",
};

let mangle_gen = 0;

// reachable blocks of `fn` in reverse postorder (entry first).  iterative
// DFS: block counts are small, but the self-hosted stack isn't deep.
function rpoBlocks(fn) {
    let visited = new Set([fn.entry]);
    let post = [];
    let stack = [{ block: fn.entry, next: 0 }];
    while (stack.length > 0) {
        let frame = stack[stack.length - 1];
        let last = frame.block.insts[frame.block.insts.length - 1];
        let targets = (last && last.targets) || [];
        if (frame.next < targets.length) {
            let succ = targets[frame.next++].block;
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
    // visitor: the active LLVMIRVisitor; we use its module, abi,
    // ejs_runtime/ejs_binops interfaces, getAtom, and ejs_globals.
    constructor(visitor) {
        this.v = visitor;
        this.abi = visitor.abi;
        this.module = visitor.module;
    }

    // declare + define every function in an EIR module; returns a Map of
    // eir function name -> llvm.Function
    emitModule(eirModule) {
        let saved_insert = ir.getInsertBlock();

        let fns = new Map();
        for (let fn of eirModule.functions) {
            if (fns.has(fn.name))
                throw new Error(`EIR emit: duplicate function name '${fn.name}' in module`);
            let llvm_name = `_ejs_eir_${fn.name.replace(/[^A-Za-z0-9_]/g, "_")}_${mangle_gen++}`;
            let llvm_fn = types.takes_builtins(
                this.abi.createFunction(
                    this.module,
                    llvm_name,
                    this.abi.ejs_return_type,
                    this.abi.ejs_params.map((p) => p.llvm_type)
                )
            );
            llvm_fn.setInternalLinkage();
            fns.set(fn.name, llvm_fn);
        }
        this.llvm_fns = fns;

        for (let fn of eirModule.functions) this.emitFunction(fn, fns.get(fn.name));

        if (saved_insert) ir.setInsertPoint(saved_insert);
        return fns;
    }

    emitFunction(eirFn, llvmFn) {
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

        let args = llvmFn.args;
        let env = args[0];
        let this_ptr = args[1];
        let argc = args[2];
        let args_ptr = args[3];
        // rest_args / args_obj / construct_super / new_target need the raw
        // calling-convention values
        this.fn_argc = argc;
        this.fn_args_ptr = args_ptr;
        this.fn_this_ptr = this_ptr;
        this.fn_new_target = args[4];

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
            ir.setInsertPoint(this.blocks.get(b));
            for (let p of b.params) {
                if (p.isException) continue; // materialized by the landingpad below
                let phi = ir.createPhi(types.EjsValue, b.predEdges.length, `p_${p.id}`);
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
        let entry_params = eirFn.entry.params;
        // params[0] = %env, params[1] = %this, rest are JS formals
        if (entry_params.length > 0) this.values.set(entry_params[0], env);
        if (entry_params.length > 1) {
            let this_val = ir.createLoad(types.EjsValue, this_ptr, "this");
            this.values.set(entry_params[1], this_val);
        }
        for (let i = 2; i < entry_params.length; i++)
            this.values.set(entry_params[i], this.emitArgLoad(argc, args_ptr, i - 2));
        // remember where the prologue ended; the branch into the eir entry
        // block is emitted *after* the body, because the legacy cached-
        // literal helpers append their initializing stores to the end of
        // whatever block is "entry" at the time they're first used.
        let prologue_end = ir.getInsertBlock();

        // emit every block's instructions
        for (let b of order) {
            ir.setInsertPoint(this.blocks.get(b));
            for (let inst of b.insts) this.emitInst(inst);
        }

        ir.setInsertPoint(prologue_end);
        ir.createBr(this.blocks.get(eirFn.entry));
        ir.setInsertPoint(entry_bb);
        ir.createBr(prologue_bb);

        this.v.currentFunction = saved_function;
        return llvmFn;
    }

    // args[i] if i < argc, else undefined -- guarded load with a phi join
    emitArgLoad(argc, args_ptr, i) {
        let load_bb = new llvm.BasicBlock(`arg${i}_load`, this.llvmFn);
        let join_bb = new llvm.BasicBlock(`arg${i}_join`, this.llvmFn);
        let from_bb = ir.getInsertBlock();

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

    emitCatchPrologue(eirBlock) {
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

        let exc_param = eirBlock.params[0];
        this.values.set(exc_param, val);
    }

    maxOutgoingArgs(eirFn) {
        let max = 0;
        eirFn.forEachInst((inst) => {
            if (inst.op === "call") max = Math.max(max, inst.operands.length - 2);
            else if (inst.op === "construct" || inst.op === "construct_super")
                max = Math.max(max, inst.operands.length - 1);
            else if (inst.op === "construct_super_apply" || inst.op === "construct_apply")
                max = Math.max(max, 1);
            else if (inst.op === "make_array" || inst.op === "array_from_spread")
                max = Math.max(max, inst.operands.length);
        });
        return max;
    }

    // --- helpers -------------------------------------------------------------------

    val(operand) {
        let v = this.values.get(operand);
        if (v === undefined)
            throw new Error(`EIR emit: no llvm value for %v${operand.id} (${operand.op})`);
        return v;
    }

    undef() {
        return this.v.loadUndefinedEjsValue();
    }

    call(callee, argv, name) {
        return this.abi.createCall(this.llvmFn, callee.type, callee, argv, name || "");
    }

    // same shape as the legacy opencoded module slot access: a non-inbounds
    // GEP into the module global (see handleModuleSlotRef in compiler.js).
    // "%self" refers to the module being compiled.
    moduleSlotRef(moduleString, slot) {
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
    spillArgs(values) {
        for (let i = 0; i < values.length; i++) {
            let gep = ir.createGetElementPointer(
                this.scratch_type,
                this.scratch,
                [consts.int32(0), consts.int64(i)],
                `sp${i}`
            );
            ir.createStore(values[i], gep);
        }
        return ir.createGetElementPointer(
            this.scratch_type,
            this.scratch,
            [consts.int32(0), consts.int64(0)],
            "spargs"
        );
    }

    // emit a call to `callee` that respects this instruction's normal/unwind
    // targets (invoke) or is a plain call
    emitCallLike(inst, callee, argv, name) {
        if (inst.targets && inst.targets.length > 0) {
            let normal = null;
            let unwind = null;
            for (let t of inst.targets) {
                if (t.kind === "unwind") unwind = t;
                else normal = t;
            }
            this.addEdgeIncomings(inst, unwind);
            this.addEdgeIncomings(inst, normal);
            let normal_bb = this.blocks.get(normal.block);
            let unwind_bb = this.blocks.get(unwind.block);
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
    addEdgeIncomings(inst, target) {
        if (!target) return;
        let src_bb = ir.getInsertBlock();
        let params = target.block.params;
        let arg_base = target.block.isCatch ? 1 : 0;
        for (let i = 0; i < target.args.length; i++) {
            let param = params[arg_base + i];
            let phi = this.phis.get(param);
            if (!phi) throw new Error("EIR emit: edge argument for missing phi");
            phi.addIncoming(this.val(target.args[i]), src_bb);
        }
    }

    // --- instruction emission -----------------------------------------------------------

    emitInst(inst) {
        let rt = this.v.ejs_runtime;

        switch (inst.op) {
            case "const": {
                let v;
                switch (inst.imms.kind) {
                    case "number":
                        v = this.v.loadDoubleEjsValue(inst.imms.value);
                        break;
                    case "atom":
                        v = this.v.getAtom(String(inst.imms.value));
                        break;
                    case "boolean":
                        v = this.v.loadBoolEjsValue(inst.imms.value);
                        break;
                    case "undefined":
                        v = this.undef();
                        break;
                    case "null":
                        v = this.v.loadNullEjsValue();
                        break;
                    default:
                        throw new Error(`EIR emit: const kind ${inst.imms.kind}`);
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
                let key = this.v.getAtom(String(inst.imms.atom));
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
                let key = this.v.getAtom(String(inst.imms.atom));
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
                let moduleString = inst.imms.module;
                let module_global;
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
                let slot_ref = this.moduleSlotRef(inst.imms.module, inst.imms.slot);
                this.values.set(inst, ir.createLoad(types.EjsValue, slot_ref, "module_slot"));
                return;
            }
            case "module_slot_store": {
                let slot_ref = this.moduleSlotRef(inst.imms.module, inst.imms.slot);
                ir.createStore(this.val(inst.operands[0]), slot_ref);
                this.values.set(inst, this.val(inst.operands[0]));
                return;
            }

            case "get_global": {
                let key = this.v.getAtom(String(inst.imms.atom));
                return this.emitCallLike(inst, rt.global_getprop, [key], "getglobal");
            }
            case "set_global": {
                let key = this.v.getAtom(String(inst.imms.atom));
                return this.emitCallLike(
                    inst,
                    rt.global_setprop,
                    [key, this.val(inst.operands[0])],
                    "setglobal"
                );
            }

            case "make_env": {
                let rv = this.call(rt.make_closure_env, [consts.int32(inst.imms.size)], "env");
                this.values.set(inst, rv);
                return;
            }
            case "env_load": {
                let ref = this.call(
                    rt.get_env_slot_ref,
                    [this.val(inst.operands[0]), consts.int32(inst.imms.slot)],
                    "slotref"
                );
                this.values.set(inst, ir.createLoad(types.EjsValue, ref, "slot"));
                return;
            }
            case "env_store": {
                let ref = this.call(
                    rt.get_env_slot_ref,
                    [this.val(inst.operands[0]), consts.int32(inst.imms.slot)],
                    "slotref"
                );
                ir.createStore(this.val(inst.operands[1]), ref);
                this.values.set(inst, this.val(inst.operands[1]));
                return;
            }
            case "make_closure": {
                let target = this.llvm_fns.get(inst.imms.fn);
                if (!target) throw new Error(`EIR emit: unknown closure target ${inst.imms.fn}`);
                let name = this.v.getAtom(
                    String(inst.imms.name !== undefined ? inst.imms.name : inst.imms.fn)
                );
                let rv = this.call(
                    rt.make_closure,
                    [this.val(inst.operands[0]), name, target],
                    "closure"
                );
                this.values.set(inst, rv);
                return;
            }

            case "call": {
                if (inst.imms.direct) {
                    let target = this.llvm_fns.get(inst.imms.direct);
                    if (!target)
                        throw new Error(`EIR emit: unknown direct callee ${inst.imms.direct}`);
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
                if (inst.imms.indices === undefined) {
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
                    [consts.int64(inst.imms.len), consts.bool(true)],
                    "arr"
                );
                this.values.set(inst, arr);
                for (let i = 0; i < elems.length; i++) {
                    let key = this.v.loadDoubleEjsValue(inst.imms.indices[i]);
                    this.call(rt.object_setprop, [arr, key, elems[i]], "");
                }
                return arr;
            }
            case "define_accessor": {
                // flags 0x19 = enumerable | configurable, matching the
                // legacy visitObjectExpression
                let key = this.v.getAtom(String(inst.imms.atom));
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
                    this.v.ejs_globals.Object_prototype,
                    "objproto"
                );
                let obj = this.call(rt.object_create, [proto], "obj");
                this.values.set(inst, obj);
                for (let i = 0; i < inst.operands.length; i++) {
                    let key = this.v.getAtom(String(inst.imms.keys[i]));
                    this.call(rt.object_setprop, [obj, key, this.val(inst.operands[i])], "");
                }
                return;
            }

            // --- control flow ---------------------------------------------------

            case "br": {
                let t = inst.targets[0];
                this.addEdgeIncomings(inst, t);
                ir.createBr(this.blocks.get(t.block));
                return;
            }
            case "cond_br": {
                let cond = this.val(inst.operands[0]);
                // prop_iter_next produces the runtime's i8 EJSBool; every
                // other condition source (to_boolean) is already an i1
                if (inst.operands[0].op === "prop_iter_next")
                    cond = ir.createICmpEq(cond, consts.True(), "moreleft_i1");
                this.addEdgeIncomings(inst, inst.targets[0]);
                this.addEdgeIncomings(inst, inst.targets[1]);
                ir.createCondBr(
                    cond,
                    this.blocks.get(inst.targets[0].block),
                    this.blocks.get(inst.targets[1].block)
                );
                return;
            }
            case "return": {
                this.abi.createRet(this.llvmFn, this.val(inst.operands[0]));
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
                        this.blocks.get(unwind.block),
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

            case "make_regexp": {
                let source = consts.string(ir, inst.imms.source);
                let flags = consts.string(ir, inst.imms.flags);
                return this.emitCallLike(inst, rt.regexp_new_utf8, [source, flags], "regexp");
            }

            case "rest_args": {
                // rest = argc > index ? array_new_copy(argc - index, args + index)
                //                     : array_new_copy(0, args)
                // (count of zero never dereferences the pointer, so the
                // select keeps this branch-free)
                let index = inst.imms.index;
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
                let callee = rt[inst.imms.name];
                if (!callee)
                    throw new Error(`EIR emit: no runtime function '${inst.imms.name}'`);
                let argv = inst.operands.map((o) => this.val(o));
                if (inst.imms.void) {
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

            default: {
                // generic binops / unops through the runtime interfaces
                let binop = binop_for_op[inst.op];
                if (binop) {
                    let callee = this.v.ejs_binops[binop];
                    if (!callee) throw new Error(`EIR emit: no binop interface for ${binop}`);
                    return this.emitCallLike(
                        inst,
                        callee,
                        [this.val(inst.operands[0]), this.val(inst.operands[1])],
                        "binres"
                    );
                }
                let unop = unop_for_op[inst.op];
                if (unop) {
                    let callee = this.v.ejs_runtime[`unop${unop}`];
                    if (!callee) throw new Error(`EIR emit: no unop interface for ${unop}`);
                    return this.emitCallLike(inst, callee, [this.val(inst.operands[0])], "unres");
                }
                throw new Error(`EIR emit: unhandled opcode '${inst.op}'`);
            }
        }
    }
}
