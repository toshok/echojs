/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

import * as llvm from "@llvm";

import { preEIRConvert as pre_eir_convert } from "./desugar";
import * as types from "./types";
import * as consts from "./consts";
import * as runtime from "./runtime";
import * as debug from "./debug";

import * as b from "./ast-builder";
import { startGenerator } from "./echo-util";

import { ABI } from "./abi";
import { SRetABI } from "./sret-abi";
import { collectEIRToplevel } from "./eir/integrate";
import type { ModuleAccessor } from "./eir/integrate";
import { EIREmitter, VisitorSurface } from "./eir/emit";
import { runTypeAnalysisProbe } from "./eir/oracle";
import type * as e from "./estree";
import type { CompilerOptions } from "./options";
import type { ModuleInfo, JSModuleInfo } from "./module-info";
import type { Triple } from "./triple";
import type { RuntimeInterface } from "./runtime";

const ir = llvm.IRBuilder;

const hasOwn = Object.prototype.hasOwnProperty;

// the state emitModuleInfo/emitModuleResolution thread between them
class LLVMIRVisitor implements VisitorSurface {
    module: llvm.Module;
    filename: string;
    triple: Triple;
    options: CompilerOptions;
    abi: ABI;
    allModules: Map<string, ModuleInfo>;
    this_module_info: JSModuleInfo;
    dibuilder: llvm.DIBuilder | undefined;
    difile: llvm.DIFile | undefined;
    idgen: () => number;
    genRecordId?: () => number;
    llvm_intrinsics: { gcroot: () => llvm.EjsFunction };
    ejs_runtime: RuntimeInterface;
    ejs_binops: Record<string, llvm.EjsFunction>;
    ejs_atoms: Record<string, llvm.GlobalVariable>;
    ejs_globals: Record<string, llvm.GlobalVariable>;
    ejs_symbols: Record<string, llvm.GlobalVariable>;
    module_atoms: Map<string, llvm.GlobalVariable>;
    // the module's interned guard shapes (imms.shape key
    // -> the i32 shape-index global + its ordered fields), filled by the
    // EIR emitter's has_shape lowering and flushed into the literal-init
    // function by emitShapeInterns (the atom-table precedent)
    module_shapes: Map<
        string,
        { global: llvm.GlobalVariable; fields: { name: string; repr: string }[] }
    >;
    literalInitializationFunction: llvm.EjsFunction;
    literalInitializationDebugInfo: llvm.DISubprogram | undefined;
    literalInitializationBB: llvm.BasicBlock;
    currentFunction: llvm.EjsFunction | null = null;
    // module scaffolding state (set by emitModuleInfo / emitEIRToplevel)
    this_module_global!: llvm.GlobalVariable;
    this_module_type!: llvm.StructType;
    this_module_initted!: llvm.GlobalVariable;
    import_module_globals!: Map<string, llvm.GlobalVariable>;
    resolve_modules_bb!: llvm.BasicBlock;
    toplevel_body_bb!: llvm.BasicBlock;
    toplevel_function!: llvm.EjsFunction;
    eir_toplevel_entry_bb: llvm.BasicBlock | null = null;
    eir_emitter?: EIREmitter;
    eir_emitted?: Map<import("./eir/ir").Module, Map<string, llvm.EjsFunction>>;
    eir_toplevel_fns!: Map<string, llvm.EjsFunction>;
    // the shape-intern init function (null when the
    // module guards no shapes), built by emitShapeInterns and called by
    // emitModuleResolution after literal initialization
    shape_init_function: llvm.EjsFunction | null = null;

    constructor(
        module: llvm.Module,
        filename: string,
        triple: Triple,
        options: CompilerOptions,
        abi: ABI,
        allModules: Map<string, ModuleInfo>,
        this_module_info: JSModuleInfo,
        dibuilder: llvm.DIBuilder | undefined,
        difile: llvm.DIFile | undefined
    ) {
        this.module = module;
        this.filename = filename;
        this.triple = triple;
        this.options = options;
        this.abi = abi;
        this.allModules = allModules;
        this.this_module_info = this_module_info;
        this.dibuilder = dibuilder;
        this.difile = difile;

        this.idgen = startGenerator();

        if (this.options.record_types) this.genRecordId = startGenerator();

        this.llvm_intrinsics = {
            gcroot: () => module.getOrInsertIntrinsic("@llvm.gcroot"),
        };

        this.ejs_runtime = runtime.createInterface(module, this.abi);
        this.ejs_binops = runtime.createBinopsInterface(module, this.abi);
        this.ejs_atoms = runtime.createAtomsInterface(module);
        this.ejs_globals = runtime.createGlobalsInterface(module);
        this.ejs_symbols = runtime.createSymbolsInterface(module);

        this.module_atoms = new Map();
        this.module_shapes = new Map();

        const init_function_name = `_ejs_module_init_string_literals_${this.filename}`;
        this.literalInitializationFunction = this.module.getOrInsertFunction(
            init_function_name,
            types.Void,
            []
        );

        if (this.options.debug)
            this.literalInitializationDebugInfo = this.dibuilder!.createFunction(
                this.difile!,
                init_function_name,
                init_function_name,
                this.difile!,
                0,
                false,
                true,
                0,
                0,
                true,
                this.literalInitializationFunction
            );

        // this function is only ever called by this module's toplevel
        this.literalInitializationFunction.setInternalLinkage();


        let entry_bb = new llvm.BasicBlock("entry", this.literalInitializationFunction);
        let return_bb = new llvm.BasicBlock("return", this.literalInitializationFunction);

        if (this.options.debug)
            ir.setCurrentDebugLocation(
                llvm.DebugLoc.get(0, 0, this.literalInitializationDebugInfo!)
            );

        this.doInsideBBlock(entry_bb, () => {
            ir.createBr(return_bb);
        });
        this.doInsideBBlock(return_bb, () => {
            //this.createCall this.ejs_runtime.log, [consts.string(ir, 'done with literal initialization')], ''
            ir.createRetVoid();
        });

        this.literalInitializationBB = entry_bb;
    }

    // lots of helper methods

    emitModuleInfo(): void {
        this.this_module_type = types.getModuleSpecificType(
            this.this_module_info.module_name,
            this.this_module_info.slot_num
        );

        this.this_module_global = new llvm.GlobalVariable(
            this.module,
            this.this_module_type,
            this.this_module_info.module_name,
            llvm.Constant.getAggregateZero(this.this_module_type),
            true
        );
        this.import_module_globals = new Map();
        for (let import_module_string of this.this_module_info.importList) {
            const import_module_info = this.allModules.get(import_module_string)!;
            if (!import_module_info.isNative())
                this.import_module_globals.set(
                    import_module_string,
                    new llvm.GlobalVariable(
                        this.module,
                        types.EjsModule,
                        import_module_info.module_name,
                        null,
                        true
                    )
                );
        }
        this.this_module_initted = new llvm.GlobalVariable(
            this.module,
            types.Bool,
            `${this.this_module_info.module_name}_initialized`,
            consts.False(),
            false
        );
    }

    emitModuleResolution(module_accessors: ModuleAccessor[]): llvm.Value {
        // this.loadUndefinedEjsValue depends on this
        this.currentFunction = this.toplevel_function;

        ir.setInsertPoint(this.resolve_modules_bb);
        if (this.options.debug)
            ir.setCurrentDebugLocation(llvm.DebugLoc.get(0, 0, this.currentFunction!.debug_info!));

        let uninitialized_bb = new llvm.BasicBlock("module_uninitialized", this.toplevel_function);
        let initialized_bb = new llvm.BasicBlock("module_initialized", this.toplevel_function);

        let load_init_flag = ir.createLoad(types.Bool, this.this_module_initted, "load_init_flag");
        let load_init_cmp = ir.createICmpEq(load_init_flag, consts.False(), "load_init_cmp");

        ir.createCondBr(load_init_cmp, uninitialized_bb, initialized_bb);

        ir.setInsertPoint(uninitialized_bb);
        ir.createStore(consts.True(), this.this_module_initted);

        ir.createCall(
            this.literalInitializationFunction.type,
            this.literalInitializationFunction,
            [],
            ""
        );

        // intern this module's guard shapes right after
        // the atoms they name are initialized
        if (this.shape_init_function)
            ir.createCall(this.shape_init_function.type, this.shape_init_function, [], "");

        // fill in the information we know about this module
        //  our name
        let name_slot = ir.createInBoundsGetElementPointer(
            this.this_module_type,
            this.this_module_global,
            [consts.int32(0), consts.int32(1)],
            "name_slot"
        );
        ir.createStore(consts.string(ir, this.this_module_info.path), name_slot);

        //  num_exports
        let num_exports_slot = ir.createInBoundsGetElementPointer(
            this.this_module_type,
            this.this_module_global,
            [consts.int32(0), consts.int32(2)],
            "num_exports_slot"
        );
        ir.createStore(consts.int32(this.this_module_info.slot_num), num_exports_slot);

        // define our accessor properties.  getter/setter are EIR function
        // names, resolved against the toplevel module's emitted functions
        for (let accessor of module_accessors) {
            let get_func =
                (accessor.getter && this.eir_toplevel_fns.get(accessor.getter)) ||
                consts.Null(types.EjsClosureFunc);
            let set_func =
                (accessor.setter && this.eir_toplevel_fns.get(accessor.setter)) ||
                consts.Null(types.EjsClosureFunc);
            let module_arg = ir.createPointerCast(
                this.this_module_global,
                types.EjsModule.pointerTo(),
                ""
            );
            ir.createCall(
                this.ejs_runtime.module_add_export_accessors.type,
                this.ejs_runtime.module_add_export_accessors,
                [module_arg, consts.string(ir, accessor.key), get_func, set_func],
                ""
            );
        }

        for (let import_module_string of this.this_module_info.importList) {
            let import_module = this.import_module_globals.get(import_module_string);
            if (import_module) {
                this.createCall(this.ejs_runtime.module_resolve, [import_module], "");
            }
        }

        ir.createBr(this.toplevel_body_bb);

        ir.setInsertPoint(initialized_bb);
        let rv = this.createRet(this.loadUndefinedEjsValue());

        // an EIR-owned toplevel defers its entry branch to here: all the
        // cached-literal initializers this function will ever append to
        // entry_bb have been appended by now
        if (this.eir_toplevel_entry_bb) {
            ir.setInsertPoint(this.eir_toplevel_entry_bb);
            ir.createBr(this.resolve_modules_bb);
            this.eir_toplevel_entry_bb = null;
        }
        return rv;
    }

    // result should be the landingpad's value
    doInsideBBlock(bb: llvm.BasicBlock, f: () => void): void {
        const saved = ir.getInsertBlock();
        ir.setInsertPoint(bb);
        f();
        ir.setInsertPoint(saved);
    }

    createEjsValueLoad(value: llvm.Value, name: string): llvm.Value {
        const rv = ir.createLoad(types.EjsValue, value, name) as llvm.AllocaInst;
        rv.setAlignment(8);
        return rv;
    }

    loadCachedEjsValue(name: string, init: (alloca: llvm.AllocaInst) => void): llvm.Value {
        let alloca_name = `${name}_alloca`;
        let load_name = `${name}_load`;

        // per-function alloca cache, dynamic-keyed on the llvm function
        // (matching the historical direct-property scheme)
        const fn = this.currentFunction!;
        const cache = fn as unknown as Record<string, llvm.AllocaInst | undefined>;
        let alloca = cache[alloca_name];
        if (!alloca) {
            const fresh = this.createAlloca(fn, types.EjsValue, alloca_name);
            cache[alloca_name] = fresh;
            this.doInsideBBlock(fn.entry_bb!, () => init(fresh));
            alloca = fresh;
        }

        return ir.createLoad(types.EjsValue, alloca, load_name);
    }

    loadBoolEjsValue(n: boolean): llvm.Value {
        const rv = this.loadCachedEjsValue(String(n), (alloca) => {
            let alloca_as_int64 = ir.createBitCast(
                alloca,
                types.Int64.pointerTo(),
                "alloca_as_pointer"
            );
            if (n)
                ir.createStore(
                    consts.ejsval_true(this.triple.pointerSize() == 32),
                    alloca_as_int64
                );
            else
                ir.createStore(
                    consts.ejsval_false(this.triple.pointerSize() == 32),
                    alloca_as_int64
                );
        });
        rv._ejs_returns_ejsval_bool = true;
        return rv;
    }

    loadDoubleEjsValue(n: number): llvm.Value {
        // -0 stringifies as "0": without the special case it would share
        // +0's cache slot (whichever the function emits first wins, and
        // 1/x flips sign).
        // The test is 1/n === -Infinity, NOT `n === 0 && 1/n < 0`: under
        // the self-hosted runtime `-0 === 0` is false (the strict_eq
        // tag-compare quirk, math2.js), which silently disabled the
        // special case exactly where it mattered.
        const key = 1 / n === -Infinity ? "num_-0" : `num_${n}`;
        return this.loadCachedEjsValue(key, (alloca) => this.storeDouble(alloca, n));
    }
    loadNullEjsValue(): llvm.Value {
        return this.loadCachedEjsValue("null", (alloca) => this.storeNull(alloca));
    }
    loadUndefinedEjsValue(): llvm.Value {
        return this.loadCachedEjsValue("undef", (alloca) => this.storeUndefined(alloca));
    }

    storeUndefined(alloca: llvm.AllocaInst, name?: string): llvm.Value {
        let alloca_as_int64 = ir.createBitCast(
            alloca,
            types.Int64.pointerTo(),
            "alloca_as_pointer"
        );
        if (this.triple.pointerSize() === 64)
            return ir.createStore(
                consts.int64_lowhi(0xfff90000, 0x00000000),
                alloca_as_int64,
                name
            );
        // 32 bit
        else
            return ir.createStore(
                consts.int64_lowhi(0xffffff82, 0x00000000),
                alloca_as_int64,
                name
            );
    }

    storeNull(alloca: llvm.AllocaInst, name?: string): llvm.Value {
        let alloca_as_int64 = ir.createBitCast(
            alloca,
            types.Int64.pointerTo(),
            "alloca_as_pointer"
        );
        if (this.triple.pointerSize() === 64)
            return ir.createStore(
                consts.int64_lowhi(0xfffb8000, 0x00000000),
                alloca_as_int64,
                name
            );
        // 32 bit
        else
            return ir.createStore(
                consts.int64_lowhi(0xffffff87, 0x00000000),
                alloca_as_int64,
                name
            );
    }

    storeDouble(alloca: llvm.AllocaInst, jsnum: number, name?: string): llvm.Value {
        let c = llvm.ConstantFP.getDouble(jsnum);
        let alloca_as_double = ir.createBitCast(
            alloca,
            types.Double.pointerTo(),
            "alloca_as_pointer"
        );
        return ir.createStore(c, alloca_as_double, name);
    }

    createAlloca(func: llvm.EjsFunction, type: llvm.Type, name: string): llvm.AllocaInst {
        let saved_insert_point = ir.getInsertBlock();
        ir.setInsertPointStartBB(func.entry_bb!);
        let alloca = ir.createAlloca(type, name);

        // if EjsValue was a pointer value we would be able to use an the llvm gcroot intrinsic here.  but with the nan boxing
        // we kinda lose out as the llvm IR code doesn't permit non-reference types to be gc roots.
        // if type is types.EjsValue
        //        // EjsValues are rooted
        //        this.createCall this.llvm_intrinsics.gcroot(), [(ir.createPointerCast alloca, types.Int8Pointer.pointerTo(), 'rooted_alloca'), consts.Null types.Int8Pointer], ''

        ir.setInsertPoint(saved_insert_point);
        return alloca;
    }

    emitEIRToplevel(n: e.FunctionDeclaration): llvm.EjsFunction {
        let insertBlock = ir.getInsertBlock();

        if (!this.eir_emitter) this.eir_emitter = new EIREmitter(this);
        if (!this.eir_emitted) this.eir_emitted = new Map();
        let eir_fns = this.eir_emitted.get(n.eir_module!);
        if (!eir_fns) {
            eir_fns = this.eir_emitter.emitModule(n.eir_module!);
            this.eir_emitted.set(n.eir_module!, eir_fns);
        }
        // export accessors resolve by name against this map (see
        // emitModuleResolution)
        this.eir_toplevel_fns = eir_fns;
        const target = eir_fns.get(n.eir_main!)!;

        const ir_func = n.ir_func!;
        this.currentFunction = ir_func;
        let entry_bb = new llvm.BasicBlock("entry", ir_func);
        ir_func.entry_bb = entry_bb; // cached-literal helpers want this
        ir_func.literalAllocas = Object.create(null);
        ir_func.topScope = new Map();

        let body_bb = new llvm.BasicBlock("body", ir_func);
        ir.setInsertPoint(body_bb);
        let args = ir_func.args;
        let rv = this.abi.createCall(
            ir_func,
            target.type,
            target,
            [args[0]!, args[1]!, args[2]!, args[3]!, args[4]!],
            "eir_toplevel_result"
        );
        this.abi.createRet(ir_func, rv);

        // emitModuleResolution wires resolve_modules_bb -> body_bb.  the
        // entry block's branch is emitted THERE, at the very end: the
        // cached-literal helpers append their initializing stores to
        // entry_bb, and nothing may follow a terminator.
        this.resolve_modules_bb = new llvm.BasicBlock("resolve_modules", ir_func);
        this.toplevel_body_bb = body_bb;
        this.toplevel_function = ir_func;
        this.eir_toplevel_entry_bb = entry_bb;

        this.currentFunction = null;
        if (insertBlock) ir.setInsertPoint(insertBlock);
        return ir_func;
    }

    // an EIR-owned function: emit its EIR module (once) and fill this
    // function's body with a forwarding call.  closure creation and env
    // plumbing stay entirely on the legacy side; the thunk just hands the
    // builtin arguments through.
    createRet(x: llvm.Value): llvm.Value {
        //this.createCall this.ejs_runtime.log, [consts.string(ir, `leaving ${this.currentFunction.name}`)], ''
        return this.abi.createRet(this.currentFunction!, x);
    }

    generateUCS2(id: number, jsstr: string): llvm.GlobalVariable {
        let ucsArrayType = llvm.ArrayType.get(types.JSChar, jsstr.length + 1);
        let array_data = [];
        for (let i = 0, e = jsstr.length; i < e; i++)
            array_data.push(consts.jschar(jsstr.charCodeAt(i)));
        array_data.push(consts.jschar(0));
        let array = llvm.ConstantArray.get(ucsArrayType, array_data);
        let arrayglobal = new llvm.GlobalVariable(
            this.module,
            ucsArrayType,
            `ucs2-${id}`,
            array,
            false
        );
        arrayglobal.setAlignment(8);
        return arrayglobal;
    }

    generateEJSPrimString(id: number, _len?: number): llvm.GlobalVariable {
        let strglobal = new llvm.GlobalVariable(
            this.module,
            types.EjsPrimString,
            `primstring-${id}`,
            llvm.Constant.getAggregateZero(types.EjsPrimString),
            false
        );
        strglobal.setAlignment(8);
        return strglobal;
    }

    generateEJSValueForString(id: number | string): llvm.GlobalVariable {
        let name = `ejsval-${id}`;
        let strglobal = new llvm.GlobalVariable(
            this.module,
            types.EjsValue,
            name,
            llvm.Constant.getAggregateZero(types.EjsValue),
            false
        );
        strglobal.setAlignment(8);
        let val = this.module.getOrInsertGlobal(name, types.EjsValue);
        val.setAlignment(8);
        return val;
    }

    addStringLiteralInitialization(
        name: string,
        ucs2: llvm.GlobalVariable,
        primstr: llvm.GlobalVariable,
        val: llvm.GlobalVariable,
        len: number
    ): void {
        let saved_insert_point = ir.getInsertBlock();

        ir.setInsertPointStartBB(this.literalInitializationBB);

        let saved_debug_loc;
        if (this.options.debug) {
            saved_debug_loc = ir.getCurrentDebugLocation();
            ir.setCurrentDebugLocation(
                llvm.DebugLoc.get(0, 0, this.literalInitializationDebugInfo!)
            );
        }

        let strname = consts.string(ir, name);

        let arg0 = strname;
        let arg1 = val;
        let arg2 = primstr;
        let arg3 = ir.createInBoundsGetElementPointer(
            types.JSChar.pointerTo(),
            ucs2,
            [consts.int32(0), consts.int32(0)],
            "ucs2"
        );

        ir.createCall(
            this.ejs_runtime.init_string_literal.type,
            this.ejs_runtime.init_string_literal,
            [arg0, arg1, arg2, arg3, consts.int32(len)],
            ""
        );
        ir.setInsertPoint(saved_insert_point);
        if (this.options.debug) ir.setCurrentDebugLocation(saved_debug_loc!);
    }

    getAtom(str: string): llvm.Value {
        // check if it's an atom (a runtime library constant) first of all
        if (hasOwn.call(this.ejs_atoms, str))
            return this.createEjsValueLoad(this.ejs_atoms[str]!, `${str}_atom_load`);

        // if it's not, we create a constant and embed it in this module
        if (!this.module_atoms.has(str)) {
            let literalId = this.idgen();
            let ucs2_data = this.generateUCS2(literalId, str);
            let primstring = this.generateEJSPrimString(literalId, str.length);
            let ejsval = this.generateEJSValueForString(str);
            this.module_atoms.set(str, ejsval);
            this.addStringLiteralInitialization(str, ucs2_data, primstring, ejsval, str.length);
        }

        return this.createEjsValueLoad(this.module_atoms.get(str)!, "literal_load");
    }

    createCall(callee: llvm.EjsFunction, argv: llvm.Value[], callname: string): llvm.Value {
        // the module scaffolding this visitor still emits never runs
        // inside a protected region; EIR-emitted code manages its own
        // invoke/landingpad pairs (see eir/emit.js)
        return this.abi.createCall(this.currentFunction!, callee.type, callee, argv, callname);
    }

    emitEjsvalFromPtr(ptr: llvm.Value, prefix: string): llvm.Value {
        if (this.triple.pointerSize() === 64) {
            let fromptr_alloca = this.createAlloca(
                this.currentFunction!,
                types.EjsValue,
                `${prefix}_ejsval`
            );
            let intval = ir.createPtrToInt(ptr, types.Int64, `${prefix}_intval`);
            let payload = ir.createOr(
                intval,
                consts.int64_lowhi(0xfffc0000, 0x00000000),
                `${prefix}_payload`
            );
            let alloca_as_int64 = ir.createBitCast(
                fromptr_alloca,
                types.Int64.pointerTo(),
                `${prefix}_alloca_asptr`
            );
            ir.createStore(payload, alloca_as_int64, `${prefix}_store`);
            return ir.createLoad(types.EjsValue, fromptr_alloca, `${prefix}_load`);
        } else {
            throw new Error("emitEjsvalTo not implemented for this case");
        }
    }

    getEjsvalBits(arg: llvm.Value): llvm.Value {
        const fn = this.currentFunction!;
        const bits_alloca = fn.bits_alloca ?? this.createAlloca(fn, types.EjsValue, "bits_alloca");

        ir.createStore(arg, bits_alloca);
        const bits_ptr = ir.createBitCast(bits_alloca, types.Int64.pointerTo(), "bits_ptr");
        if (!fn.bits_alloca) fn.bits_alloca = bits_alloca;
        return ir.createLoad(types.Int64, bits_ptr, "bits_load");
    }

    createEjsvalICmpULt(arg: llvm.Value, i64_const: llvm.Constant, name: string): llvm.Value {
        return ir.createICmpULt(this.getEjsvalBits(arg), i64_const, name);
    }
    // The low tier's NaN-box transfers.  Doubles are stored RAW in the
    // ejsval (see storeDouble): unbox/box are pure bit reinterpretations
    // through the same cached alloca getEjsvalBits uses.  Target layout
    // knowledge stays here, beside isNumber.
    unboxDouble(val: llvm.Value): llvm.Value {
        const fn = this.currentFunction!;
        const alloca = fn.bits_alloca ?? this.createAlloca(fn, types.EjsValue, "bits_alloca");
        ir.createStore(val, alloca);
        const dbl_ptr = ir.createBitCast(alloca, types.Double.pointerTo(), "dbl_ptr");
        if (!fn.bits_alloca) fn.bits_alloca = alloca;
        return ir.createLoad(types.Double, dbl_ptr, "unboxed_f64");
    }
    boxDouble(dbl: llvm.Value): llvm.Value {
        const fn = this.currentFunction!;
        const alloca = fn.bits_alloca ?? this.createAlloca(fn, types.EjsValue, "bits_alloca");
        const dbl_ptr = ir.createBitCast(alloca, types.Double.pointerTo(), "dbl_ptr");
        ir.createStore(dbl, dbl_ptr);
        if (!fn.bits_alloca) fn.bits_alloca = alloca;
        return ir.createLoad(types.EjsValue, alloca, "boxed_f64");
    }
    isNumber(val: llvm.Value): llvm.Value {
        if (this.triple.pointerSize() === 64) {
            return this.createEjsvalICmpULt(
                val,
                consts.int64_lowhi(0xfff80001, 0x00000000),
                "cmpresult"
            );
        } else {
            let trunc = ir.createTrunc(this.getEjsvalBits(val), types.Int32, "trunc.i");
            return ir.createICmpEq(trunc, consts.int32(-127), "cmpresult");
        }
    }

    // shape-guard target-layout helpers (beside isNumber so all
    // NaN-box knowledge stays in one place)

    // EJSVAL_IS_OBJECT: object is the topmost shifted tag, so on 64-bit a
    // single unsigned compare suffices (mirrors EJSVAL_IS_OBJECT_IMPL)
    // NOTE: OBJECT must stay the maximum ejsval tag (BigInt slotted in
    // below it) — this constant is SHIFTED_TAG_OBJECT and the check is >=
    isObject(val: llvm.Value): llvm.Value {
        if (this.triple.pointerSize() === 64) {
            return ir.createICmpUGE(
                this.getEjsvalBits(val),
                // SHIFTED_TAG_OBJECT (tag 0x1FFFA — object moved up when
                // BIGINT took 0x09)
                consts.int64_lowhi(0xfffd0000, 0x00000000),
                "isobj"
            );
        } else {
            // 32-bit: tag compare, the isNumber trunc convention
            let trunc = ir.createTrunc(this.getEjsvalBits(val), types.Int32, "trunc.i");
            return ir.createICmpEq(trunc, consts.int32(-118) /* 0xFFFFFF8A */, "isobj");
        }
    }

    // EJSVAL_TO_OBJECT: payload-mask the bits and reinterpret as EJSObject*.
    // Only valid under a passed isObject check.
    objectPointer(val: llvm.Value): llvm.Value {
        if (this.triple.pointerSize() !== 64)
            throw new Error("objectPointer not implemented for 32-bit targets");
        const payload = ir.createAnd(
            this.getEjsvalBits(val),
            consts.int64_lowhi(0x00007fff, 0xffffffff),
            "obj_payload"
        );
        return ir.createIntToPtr(payload, types.EjsObject.pointerTo(), "objptr");
    }

    // is this value's payload inside the nursery?  The seam
    // contract (ejs-gc.h EJSHeapContext) fixes the layout: 18 i64 words —
    // bump[5], limit[5], nursery_base (word 10), nursery_end (word 11).
    // A double's payload can false-positive into the range; the out-of-
    // line barrier re-filters, so the inline check only needs to be
    // sound-when-true-called.  With the nursery off both bounds are 0
    // and the check is constant-false.
    heap_ctx_global: llvm.GlobalVariable | null = null;
    heapContextGlobal(): llvm.GlobalVariable {
        if (!this.heap_ctx_global)
            this.heap_ctx_global = new llvm.GlobalVariable(
                this.module,
                llvm.ArrayType.get(types.Int64, 18),
                "_ejs_heap",
                null,
                true
            );
        return this.heap_ctx_global;
    }
    // the runtime's accessor epoch (ejs-object.h): zero while nothing
    // user-installed can intercept a [[Set]] through a fresh object's
    // prototype chain.  The check is one load + compare-to-zero.
    accessor_epoch_global: llvm.GlobalVariable | null = null;
    emitAccessorEpochCheck(): llvm.Value {
        if (!this.accessor_epoch_global)
            this.accessor_epoch_global = new llvm.GlobalVariable(
                this.module,
                types.Int64,
                "_ejs_accessor_epoch",
                null,
                true
            );
        const epoch = ir.createLoad(types.Int64, this.accessor_epoch_global, "accessor_epoch");
        return ir.createICmpEq(epoch, consts.int64(0), "epoch_ok");
    }
    // the inline nursery allocation for closure
    // environments — bump, compare, init header/length/slots, box with
    // the CLOSUREENV tag; the slow thunk (the existing runtime call) is
    // the safepoint.  With the nursery off, bump/limit are NULL and the
    // compare always routes slow.  All layout knowledge (cell classes,
    // header bits, NaN-box tags, struct offsets) stays here with the
    // other NaN-box helpers.
    emitEnvAllocInline(n: number, slowCall: () => llvm.Value): llvm.Value {
        if (this.triple.pointerSize() !== 64) return slowCall();
        const value_size = 16 + 8 * n; // EJSClosureEnv: u64 header, u32 length(+pad), slots
        let cell_size = 16;
        while (cell_size < value_size) cell_size *= 2;
        if (cell_size > 256) return slowCall(); // LOS-routed sizes take the runtime path
        const idx = Math.log2(cell_size) - 4;   // seam word: bump[idx], limit[5+idx]

        const g = this.heapContextGlobal();
        const arr_ty = llvm.ArrayType.get(types.Int64, 18);
        const bump_p = ir.createInBoundsGetElementPointer(
            arr_ty, g, [consts.int64(0), consts.int32(idx)], "env_bump_p");
        const limit_p = ir.createInBoundsGetElementPointer(
            arr_ty, g, [consts.int64(0), consts.int32(5 + idx)], "env_limit_p");
        const bump = ir.createLoad(types.Int64, bump_p, "env_bump");
        const limit = ir.createLoad(types.Int64, limit_p, "env_limit");
        // the bindings carry no integer add: pointer arithmetic happens
        // through i8 GEPs off the bump address
        const i8 = llvm.Type.getInt8Ty();
        const bump_ptr = ir.createIntToPtr(bump, i8.pointerTo(), "env_bump_ptr");
        const byteOffset = (k: number, name: string): llvm.Value =>
            ir.createInBoundsGetElementPointer(i8, bump_ptr, [consts.int64(k)], name);
        const newbump = ir.createPtrToInt(
            byteOffset(cell_size, "env_newbump_ptr"), types.Int64, "env_newbump");
        // newbump <= limit, spelled with the comparison the bindings have
        const fits = ir.createICmpUGE(limit, newbump, "env_fits");

        const fast_bb = new llvm.BasicBlock("env_alloc_fast", this.currentFunction!);
        const slow_bb = new llvm.BasicBlock("env_alloc_slow", this.currentFunction!);
        const join_bb = new llvm.BasicBlock("env_alloc_join", this.currentFunction!);
        const from_bb = ir.getInsertBlock()!;
        ir.createCondBr(fits, fast_bb, slow_bb);

        ir.setInsertPoint(fast_bb);
        ir.createStore(newbump, bump_p);
        // header: EJS_SCAN_TYPE_CLOSUREENV | YOUNG (bit 57)
        const hdr_ptr = ir.createBitCast(bump_ptr, types.Int64.pointerTo(), "env_hdr_p");
        ir.createStore(consts.int64_lowhi(0x02000000, 0x00000008), hdr_ptr);
        // length at +8 (u32)
        const len_ptr = ir.createBitCast(
            byteOffset(8, "env_len_addr"), types.Int32.pointerTo(), "env_len_p");
        ir.createStore(consts.int32(n), len_ptr);
        // slots at +16: undefined-filled, exactly what _ejs_closure_init does
        const undef = this.loadUndefinedEjsValue();
        for (let i = 0; i < n; i++) {
            const s_ptr = ir.createBitCast(
                byteOffset(16 + 8 * i, `env_slot${i}_addr`),
                types.EjsValue.pointerTo(), `env_slot${i}_p`);
            ir.createStore(undef, s_ptr);
        }
        // box: CLOSUREENV shifted tag (0x1FFF6 << 47)
        const boxed_bits = ir.createOr(
            bump, consts.int64_lowhi(0xfffb0000, 0x00000000), "env_boxed_bits");
        const box_alloca = this.createAlloca(this.currentFunction!, types.EjsValue, "env_box");
        const box_i64p = ir.createBitCast(box_alloca, types.Int64.pointerTo(), "env_box_i64p");
        ir.createStore(boxed_bits, box_i64p);
        const fast_env = ir.createLoad(types.EjsValue, box_alloca, "env_fast");
        const fast_end_bb = ir.getInsertBlock()!;
        ir.createBr(join_bb);

        ir.setInsertPoint(slow_bb);
        const slow_env = slowCall();
        const slow_end_bb = ir.getInsertBlock()!;
        ir.createBr(join_bb);

        ir.setInsertPoint(join_bb);
        const phi = ir.createPhi(types.EjsValue, 2, "env_alloc");
        phi.addIncoming(fast_env, fast_end_bb);
        phi.addIncoming(slow_env, slow_end_bb);
        return phi;
    }

    emitYoungCheck(val: llvm.Value): llvm.Value {
        if (this.triple.pointerSize() !== 64)
            throw new Error("emitYoungCheck not implemented for 32-bit targets");
        const g = this.heapContextGlobal();
        const arr_ty = llvm.ArrayType.get(types.Int64, 18);
        const base_p = ir.createInBoundsGetElementPointer(
            arr_ty, g, [consts.int64(0), consts.int32(10)], "nursery_base_p");
        const base = ir.createLoad(types.Int64, base_p, "nursery_base");
        const end_p = ir.createInBoundsGetElementPointer(
            arr_ty, g, [consts.int64(0), consts.int32(11)], "nursery_end_p");
        const end = ir.createLoad(types.Int64, end_p, "nursery_end");
        const payload = ir.createAnd(
            this.getEjsvalBits(val),
            consts.int64_lowhi(0x00007fff, 0xffffffff),
            "wb_payload"
        );
        const ge = ir.createICmpUGE(payload, base, "wb_ge_base");
        const lt = ir.createICmpULt(payload, end, "wb_lt_end");
        return ir.createAnd(ge, lt, "wb_young");
    }

    // the gc-frame chain head is word 17 of the seam
    // (EJSHeapContext.gc_frame_head — bump[5], limit[5], nursery
    // bounds, remset words, current_stack_end, priv precede it)
    gcFrameHeadPtr(): llvm.Value {
        const g = this.heapContextGlobal();
        const arr_ty = llvm.ArrayType.get(types.Int64, 18);
        return ir.createInBoundsGetElementPointer(
            arr_ty, g, [consts.int64(0), consts.int32(17)], "gc_frame_head_p");
    }

    // link an emitted function's gc-frame record: { prev, count,
    // slots[count] } laid out as i64 words in `frame` (an alloca).
    // Every slot is initialized to undefined — a stale slot must still
    // parse as a valid ejsval when the collector walks it.  Linking the
    // frame's address into the exported seam is also what makes the
    // alloca ESCAPE: LLVM can no longer forward pre-call slot stores to
    // post-call reloads across any external call (the
    // store-to-load-forwarding hazard the gc plan names).
    emitGCFrameLink(frame: llvm.Value, nslots: number, undef: llvm.Value): void {
        const i8 = llvm.Type.getInt8Ty();
        const base = ir.createBitCast(frame, i8.pointerTo(), "gcf_base");
        const headp = this.gcFrameHeadPtr();
        const prev = ir.createLoad(types.Int64, headp, "gcf_prev");
        const prev_p = ir.createBitCast(base, types.Int64.pointerTo(), "gcf_prev_p");
        ir.createStore(prev, prev_p);
        const count_p = ir.createBitCast(
            ir.createInBoundsGetElementPointer(i8, base, [consts.int64(8)], "gcf_count_addr"),
            types.Int64.pointerTo(), "gcf_count_p");
        ir.createStore(consts.int64(nslots), count_p);
        for (let i = 0; i < nslots; i++) {
            const slot_p = ir.createBitCast(
                ir.createInBoundsGetElementPointer(
                    i8, base, [consts.int64(16 + 8 * i)], `gcf_slot${i}_addr`),
                types.EjsValue.pointerTo(), `gcf_slot${i}_p`);
            ir.createStore(undef, slot_p);
        }
        ir.createStore(ir.createPtrToInt(base, types.Int64, "gcf_addr"), headp);
    }

    // epilogue: pop this frame off the chain
    emitGCFrameUnlink(frame: llvm.Value): void {
        const i8 = llvm.Type.getInt8Ty();
        const base = ir.createBitCast(frame, i8.pointerTo(), "gcf_base");
        const prev_p = ir.createBitCast(base, types.Int64.pointerTo(), "gcf_prev_p");
        const prev = ir.createLoad(types.Int64, prev_p, "gcf_prev");
        ir.createStore(prev, this.gcFrameHeadPtr());
    }

    // catch handler: the unwind discarded every callee frame below us —
    // re-link our own record as the head
    emitGCFrameRelink(frame: llvm.Value): void {
        const i8 = llvm.Type.getInt8Ty();
        const base = ir.createBitCast(frame, i8.pointerTo(), "gcf_base");
        ir.createStore(
            ir.createPtrToInt(base, types.Int64, "gcf_addr"), this.gcFrameHeadPtr());
    }

    // the address of gc-frame slot i, as an EjsValue*
    gcFrameSlotPtr(frame: llvm.Value, i: number): llvm.Value {
        const i8 = llvm.Type.getInt8Ty();
        const base = ir.createBitCast(frame, i8.pointerTo(), "gcf_base");
        return ir.createBitCast(
            ir.createInBoundsGetElementPointer(
                i8, base, [consts.int64(16 + 8 * i)], `gcf_slot${i}_addr`),
            types.EjsValue.pointerTo(), `gcf_slot${i}_p`);
    }

    // inline closure-env slot addressing — payload mask +
    // byte offset (EJSClosureEnv: u64 header, u32 length(+pad), slots
    // at +16).  Recomputed PER USE from the boxed env value, never
    // cached across a safepoint: when the env value itself lives in a
    // gc-frame slot, the post-safepoint reload feeds a fresh address
    // computation, so a relocated env re-derives correctly.  Replaces a
    // runtime call per env access.
    emitEnvSlotRef(env: llvm.Value, slot: number): llvm.Value {
        const i8 = llvm.Type.getInt8Ty();
        const payload = ir.createAnd(
            this.getEjsvalBits(env),
            consts.int64_lowhi(0x00007fff, 0xffffffff),
            "env_payload"
        );
        const base = ir.createIntToPtr(payload, i8.pointerTo(), "env_base");
        return ir.createBitCast(
            ir.createInBoundsGetElementPointer(
                i8, base, [consts.int64(16 + 8 * slot)], "env_slot_addr"),
            types.EjsValue.pointerTo(), "env_slot_p");
    }

    // the module's i32 shape-index global for `key`, minted on first use
    // (initialized to EJS_SHAPE_NOMATCH so a guard can never pass before
    // module init interns the real index)
    moduleShapeGlobal(
        key: string,
        fields: { name: string; repr: string }[]
    ): llvm.GlobalVariable {
        let entry = this.module_shapes.get(key);
        if (!entry) {
            const g = new llvm.GlobalVariable(
                this.module,
                types.Int32,
                `ejs_shape-${this.idgen()}`,
                consts.int32(0xffffff) /* EJS_SHAPE_NOMATCH */,
                false
            );
            entry = { global: g, fields: fields.slice() };
            this.module_shapes.set(key, entry);
        }
        return entry.global;
    }

    // flush the pending shape interns into their own init function (one
    // _ejs_shape_intern call per shape), called by emitModuleResolution
    // right after the literal-initialization call — so every atom the
    // shapes name is initialized first.  A separate function rather than
    // the literal-init one: getAtom on a not-yet-interned name restores
    // the builder to the END of the current block, which inside an
    // already-terminated block would emit past the terminator; here the
    // body block stays unterminated until the very end.  Called once,
    // after all EIR emission.
    emitShapeInterns(): llvm.EjsFunction | null {
        if (this.module_shapes.size === 0) return null;
        const saved_insert = ir.getInsertBlock();
        const saved_function = this.currentFunction;

        const fname = `_ejs_module_init_shapes_${this.filename}`;
        const fn = this.module.getOrInsertFunction(fname, types.Void, []);
        fn.setInternalLinkage();
        this.currentFunction = fn;
        const body_bb = new llvm.BasicBlock("entry", fn);
        ir.setInsertPoint(body_bb);

        for (const entry of this.module_shapes.values()) {
            const fields = entry.fields;
            const arr_ty = llvm.ArrayType.get(types.EjsValue, fields.length);
            const arr = ir.createAlloca(arr_ty, "shape_names");
            arr.setAlignment(8);
            let f64_mask = 0;
            for (let i = 0; i < fields.length; i++) {
                if (fields[i]!.repr === "f64") f64_mask |= 1 << i;
                const atom = this.getAtom(fields[i]!.name);
                const gep = ir.createGetElementPointer(
                    arr_ty,
                    arr,
                    [consts.int32(0), consts.int64(i)],
                    "shape_name_slot"
                );
                ir.createStore(atom, gep);
            }
            const base = ir.createGetElementPointer(
                arr_ty,
                arr,
                [consts.int32(0), consts.int64(0)],
                "shape_names_base"
            );
            const idx = this.createCall(
                this.ejs_runtime.shape_intern,
                [consts.int32(fields.length), base, consts.int32(f64_mask)],
                "shape_idx"
            );
            ir.createStore(idx, entry.global);
        }
        ir.createRetVoid();

        this.currentFunction = saved_function;
        if (saved_insert) ir.setInsertPoint(saved_insert);
        return fn;
    }
}

function insert_toplevel_func(tree: e.Program, moduleInfo: JSModuleInfo): e.Program {
    let toplevel = {
        type: b.FunctionDeclaration,
        id: b.identifier(moduleInfo.toplevel_function_name),
        displayName: "toplevel",
        params: [],
        defaults: [],
        body: {
            type: b.BlockStatement,
            body: tree.body,
            loc: {
                start: {
                    line: 0,
                    column: 0,
                },
            },
        },
        toplevel: true,
        generator: false,
        expression: false,
        loc: {
            start: {
                line: 0,
                column: 0,
            },
        },
    };

    tree.body = [toplevel];
    return tree;
}

export function compile(
    tree: e.Program,
    base_output_filename: string,
    source_filename: string,
    module_infos: Map<string, ModuleInfo>,
    options: CompilerOptions,
    triple: Triple
): llvm.Module {
    let abi = triple.abi();

    types.initTypes(triple.pointerSize() === 32);

    let module_filename = source_filename;

    if (module_filename.endsWith(".js")) {
        module_filename = module_filename.substring(0, module_filename.length - 3);
    }

    const this_module_info = module_infos.get(module_filename) as JSModuleInfo;

    tree = insert_toplevel_func(tree, this_module_info);

    // pipeline-agnostic desugars run before EIR collection so both
    // pipelines see their %-intrinsic output
    tree = pre_eir_convert(tree, module_filename, module_infos, options);

    // --types (the MAAM oracle): type analysis over the desugared
    // toplevel.  Must run before collectEIRToplevel, which consumes (and
    // then empties) the toplevel body.  Logs stats (and, for --types-dump,
    // per-binding types); the returned TypeOracle is not consumed by
    // codegen unless --types feeds the oracle onward; never fails the compile.
    let type_oracle = null;
    if (options.types || options.types_dump)
        type_oracle = runTypeAnalysisProbe(tree, source_filename, options.types_dump);

    // EIR is the only pipeline: a module that can't lower is a compile
    // error, not a fallback
    let lowered = collectEIRToplevel(
        tree,
        source_filename,
        module_infos,
        this_module_info,
        options,
        type_oracle
    );
    if (lowered.error) throw new Error(`${source_filename}: ${lowered.error}`);
    // telemetry: how many guarded diamonds lowering emitted, and
    // whether any oracle query missed (the node-identity canary)
    if (type_oracle) {
        // shape-guard telemetry (visible degradation):
        // counted decline reasons, additive-only on the scraped line
        const declined = lowered.shape_declined ?? {};
        const declineStr = Object.keys(declined)
            .sort()
            .map((k) => `${k}:${declined[k]}`)
            .join(",");
        console.warn(
            `--types: ${source_filename}: diamonds=${lowered.diamonds ?? 0} ` +
                `oracleQueries=${type_oracle.stats.queries} oracleUnknown=${type_oracle.stats.unknown}` +
                // specialization telemetry, present only when it ran
                (lowered.spec
                    ? ` specialized=${lowered.spec.specialized} specSites=${lowered.spec.sites}` +
                      ` specRejected=${lowered.spec.rejected}` +
                      // boundary-wrapper telemetry (additive)
                      (lowered.spec.wrapped > 0 ? ` specWrapped=${lowered.spec.wrapped}` : "") +
                      (lowered.spec.fenced > 0 ? ` specFenced=${lowered.spec.fenced}` : "")
                    : "") +
                // shape telemetry, present only when sites were consulted
                ((lowered.shape_sites ?? 0) > 0
                    ? ` shapeSites=${lowered.shape_sites} shapeGuards=${lowered.shape_guards ?? 0}` +
                      // poly-chain telemetry (additive)
                      ((lowered.shape_poly_guards ?? 0) > 0
                          ? ` shapePolyGuards=${lowered.shape_poly_guards}`
                          : "") +
                      ` shapeDeclined=${declineStr || "none"}`
                    : "") +
                // typed slot telemetry (additive)
                ((lowered.typed_loads ?? 0) > 0 || (lowered.typed_stores ?? 0) > 0
                    ? ` shapeTyped=loads:${lowered.typed_loads ?? 0},stores:${lowered.typed_stores ?? 0}`
                    : "") +
                // born-with-shape telemetry (additive)
                ((lowered.born_shaped ?? 0) > 0 || (lowered.ctor_fills ?? 0) > 0
                    ? ` bornShaped=${lowered.born_shaped ?? 0} ctorFills=${lowered.ctor_fills ?? 0}`
                    : "") +
                (Object.keys(lowered.fence_declined ?? {}).length > 0
                    ? ` fenceDeclined=${Object.keys(lowered.fence_declined!)
                          .sort()
                          .map((k) => `${k}:${lowered.fence_declined![k]}`)
                          .join(",")}`
                    : "") +
                // constructor-result sinking telemetry (additive)
                ((lowered.ctor_sunk ?? 0) > 0 ? ` ctorSunk=${lowered.ctor_sunk}` : "")
        );
    }

    const toplevel_node = tree.body[0] as e.FunctionDeclaration;
    const toplevel_name = toplevel_node.id.name;

    let module = new llvm.Module(base_output_filename);
    module.setTriple(triple.llvmTriple());
    module.setDataLayout(triple.dataLayout());

    (module as unknown as { toplevel_name: string }).toplevel_name = toplevel_name;

    let dibuilder: llvm.DIBuilder | undefined;
    let difile: llvm.DIFile | undefined;

    if (options.debug) {
        dibuilder = new llvm.DIBuilder(module);
        difile = dibuilder.createFile(source_filename + ".js", process.cwd());

        dibuilder.createCompileUnit(source_filename + ".js", process.cwd(), "ejs", true, "", 2);
    }

    // the toplevel's own llvm function — the module-scaffolding wrapper
    // that emitEIRToplevel fills and emitModuleResolution finishes
    toplevel_node.ir_name = toplevel_name;
    toplevel_node.ir_func = types.takes_builtins(
        abi.createFunction(
            module,
            toplevel_name,
            abi.ejs_return_type,
            abi.ejs_params.map((param) => param.llvm_type)
        )
    );
    if (dibuilder && difile)
        toplevel_node.ir_func.debug_info = dibuilder.createFunction(
            difile,
            toplevel_name,
            "toplevel",
            difile,
            0,
            false,
            true,
            0,
            0,
            true,
            toplevel_node.ir_func
        );

    let visitor = new LLVMIRVisitor(
        module,
        source_filename,
        triple,
        options,
        abi,
        module_infos,
        this_module_info,
        dibuilder,
        difile
    );

    if (options.debug) dibuilder!.finalize();

    visitor.emitModuleInfo();

    visitor.emitEIRToplevel(toplevel_node);

    // every has_shape has been emitted by now; flush the module's shape
    // interns into their init function —
    // emitModuleResolution calls it after literal initialization
    visitor.shape_init_function = visitor.emitShapeInterns();

    visitor.emitModuleResolution(lowered.accessors!);

    return module;
}
