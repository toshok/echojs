/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
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
import { EIREmitter } from "./eir/emit";

let ir = llvm.IRBuilder;

let hasOwn = Object.prototype.hasOwnProperty;

class LLVMIRVisitor {
    constructor(module, filename, triple, options, abi, allModules, this_module_info, dibuilder, difile) {
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

        let init_function_name = `_ejs_module_init_string_literals_${this.filename}`;
        this.literalInitializationFunction = this.module.getOrInsertFunction(
            init_function_name,
            types.Void,
            []
        );

        if (this.options.debug)
            this.literalInitializationDebugInfo = this.dibuilder.createFunction(
                this.difile,
                init_function_name,
                init_function_name,
                this.difile,
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
                llvm.DebugLoc.get(0, 0, this.literalInitializationDebugInfo)
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

    emitModuleInfo() {
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
            let import_module_info = this.allModules.get(import_module_string);
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

    emitModuleResolution(module_accessors) {
        // this.loadUndefinedEjsValue depends on this
        this.currentFunction = this.toplevel_function;

        ir.setInsertPoint(this.resolve_modules_bb);
        if (this.options.debug)
            ir.setCurrentDebugLocation(llvm.DebugLoc.get(0, 0, this.currentFunction.debug_info));

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
                this.createCall(
                    this.ejs_runtime.module_resolve,
                    [import_module],
                    "",
                    !this.ejs_runtime.module_resolve.doesNotThrow
                );
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
    doInsideBBlock(b, f) {
        let saved = ir.getInsertBlock();
        ir.setInsertPoint(b);
        f();
        ir.setInsertPoint(saved);
        return b;
    }

    createEjsValueLoad(value, name) {
        let rv = ir.createLoad(types.EjsValue, value, name);
        rv.setAlignment(8);
        return rv;
    }

    loadCachedEjsValue(name, init) {
        let alloca_name = `${name}_alloca`;
        let load_name = `${name}_load`;

        let alloca;
        if (this.currentFunction[alloca_name]) {
            alloca = this.currentFunction[alloca_name];
        } else {
            alloca = this.createAlloca(this.currentFunction, types.EjsValue, alloca_name);
            this.currentFunction[alloca_name] = alloca;
            this.doInsideBBlock(this.currentFunction.entry_bb, () => init(alloca));
        }

        return ir.createLoad(types.EjsValue, alloca, load_name);
    }

    loadBoolEjsValue(n) {
        let rv = this.loadCachedEjsValue(n, (alloca) => {
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

    loadDoubleEjsValue(n) {
        return this.loadCachedEjsValue(`num_${n}`, (alloca) => this.storeDouble(alloca, n));
    }
    loadNullEjsValue() {
        return this.loadCachedEjsValue("null", (alloca) => this.storeNull(alloca));
    }
    loadUndefinedEjsValue() {
        return this.loadCachedEjsValue("undef", (alloca) => this.storeUndefined(alloca));
    }

    storeUndefined(alloca, name) {
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

    storeNull(alloca, name) {
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

    storeDouble(alloca, jsnum, name) {
        let c = llvm.ConstantFP.getDouble(jsnum);
        let alloca_as_double = ir.createBitCast(
            alloca,
            types.Double.pointerTo(),
            "alloca_as_pointer"
        );
        return ir.createStore(c, alloca_as_double, name);
    }

    createAlloca(func, type, name) {
        let saved_insert_point = ir.getInsertBlock();
        ir.setInsertPointStartBB(func.entry_bb);
        let alloca = ir.createAlloca(type, name);

        // if EjsValue was a pointer value we would be able to use an the llvm gcroot intrinsic here.  but with the nan boxing
        // we kinda lose out as the llvm IR code doesn't permit non-reference types to be gc roots.
        // if type is types.EjsValue
        //        // EjsValues are rooted
        //        this.createCall this.llvm_intrinsics.gcroot(), [(ir.createPointerCast alloca, types.Int8Pointer.pointerTo(), 'rooted_alloca'), consts.Null types.Int8Pointer], ''

        ir.setInsertPoint(saved_insert_point);
        return alloca;
    }

    emitEIRToplevel(n) {
        let insertBlock = ir.getInsertBlock();

        if (!this.eir_emitter) this.eir_emitter = new EIREmitter(this);
        if (!this.eir_emitted) this.eir_emitted = new Map();
        let eir_fns = this.eir_emitted.get(n.eir_module);
        if (!eir_fns) {
            eir_fns = this.eir_emitter.emitModule(n.eir_module);
            this.eir_emitted.set(n.eir_module, eir_fns);
        }
        // export accessors resolve by name against this map (see
        // emitModuleResolution)
        this.eir_toplevel_fns = eir_fns;
        let target = eir_fns.get(n.eir_main);

        let ir_func = n.ir_func;
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
            [args[0], args[1], args[2], args[3], args[4]],
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
    createRet(x) {
        //this.createCall this.ejs_runtime.log, [consts.string(ir, `leaving ${this.currentFunction.name}`)], ''
        return this.abi.createRet(this.currentFunction, x);
    }

    generateUCS2(id, jsstr) {
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

    generateEJSPrimString(id) {
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

    generateEJSValueForString(id) {
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

    addStringLiteralInitialization(name, ucs2, primstr, val, len) {
        let saved_insert_point = ir.getInsertBlock();

        ir.setInsertPointStartBB(this.literalInitializationBB);

        let saved_debug_loc;
        if (this.options.debug) {
            saved_debug_loc = ir.getCurrentDebugLocation();
            ir.setCurrentDebugLocation(
                llvm.DebugLoc.get(0, 0, this.literalInitializationDebugInfo)
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
        if (this.options.debug) ir.setCurrentDebugLocation(saved_debug_loc);
    }

    getAtom(str) {
        // check if it's an atom (a runtime library constant) first of all
        if (hasOwn.call(this.ejs_atoms, str))
            return this.createEjsValueLoad(this.ejs_atoms[str], `${str}_atom_load`);

        // if it's not, we create a constant and embed it in this module
        if (!this.module_atoms.has(str)) {
            let literalId = this.idgen();
            let ucs2_data = this.generateUCS2(literalId, str);
            let primstring = this.generateEJSPrimString(literalId, str.length);
            let ejsval = this.generateEJSValueForString(str);
            this.module_atoms.set(str, ejsval);
            this.addStringLiteralInitialization(str, ucs2_data, primstring, ejsval, str.length);
        }

        return this.createEjsValueLoad(this.module_atoms.get(str), "literal_load");
    }

    createCall(callee, argv, callname) {
        // the module scaffolding this visitor still emits never runs
        // inside a protected region; EIR-emitted code manages its own
        // invoke/landingpad pairs (see eir/emit.js)
        return this.abi.createCall(this.currentFunction, callee.type, callee, argv, callname);
    }

    emitEjsvalFromPtr(ptr, prefix) {
        if (this.triple.pointerSize() === 64) {
            let fromptr_alloca = this.createAlloca(
                this.currentFunction,
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

    getEjsvalBits(arg) {
        let bits_alloca;

        if (this.currentFunction.bits_alloca) bits_alloca = this.currentFunction.bits_alloca;
        else bits_alloca = this.createAlloca(this.currentFunction, types.EjsValue, "bits_alloca");

        ir.createStore(arg, bits_alloca);
        let bits_ptr = ir.createBitCast(bits_alloca, types.Int64.pointerTo(), "bits_ptr");
        if (!this.currentFunction.bits_alloca) this.currentFunction.bits_alloca = bits_alloca;
        return ir.createLoad(types.Int64, bits_ptr, "bits_load");
    }

    createEjsvalICmpULt(arg, i64_const, name) {
        return ir.createICmpULt(this.getEjsvalBits(arg), i64_const, name);
    }
    isNumber(val) {
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
}

function insert_toplevel_func(tree, moduleInfo) {
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

export function compile(tree, base_output_filename, source_filename, module_infos, options, triple) {
    let abi = triple.abi();

    types.initTypes(triple.pointerSize() === 32);

    let module_filename = source_filename;

    if (module_filename.endsWith(".js")) {
        module_filename = module_filename.substring(0, module_filename.length - 3);
    }

    let this_module_info = module_infos.get(module_filename);

    tree = insert_toplevel_func(tree, this_module_info);

    // pipeline-agnostic desugars run before EIR collection so both
    // pipelines see their %-intrinsic output
    tree = pre_eir_convert(tree, module_filename, module_infos, options);

    // EIR is the only pipeline: a module that can't lower is a compile
    // error, not a fallback
    let lowered = collectEIRToplevel(tree, source_filename, module_infos, this_module_info, options);
    if (lowered.error) throw new Error(`${source_filename}: ${lowered.error}`);

    let toplevel_node = tree.body[0];
    let toplevel_name = toplevel_node.id.name;

    let module = new llvm.Module(base_output_filename);
    module.setTriple(triple.llvmTriple());
    module.setDataLayout(triple.dataLayout());

    module.toplevel_name = toplevel_name;

    let dibuilder;
    let difile;

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

    if (options.debug) dibuilder.finalize();

    visitor.emitModuleInfo();

    visitor.emitEIRToplevel(toplevel_node);

    visitor.emitModuleResolution(lowered.accessors);

    return module;
}
