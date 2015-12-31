/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import * as llvm          from '@llvm';
import * as path          from '@node-compat/path'

import { Stack }          from './stack-es6';
import { TreeVisitor }    from './node-visitor';

import { generate as escodegenerate } from '../external-deps/escodegen/escodegen-es6';
import { convert as closure_convert } from './closure-conversion';
import { reportError }                from './errors';
import * as optimizations             from './optimizations';
import * as types                     from './types';
import * as consts                    from './consts';
import * as runtime                   from './runtime';
import * as debug                     from './debug';

import * as b from './ast-builder';
import { startGenerator, intrinsic, is_intrinsic, is_string_literal } from './echo-util';
import { ExitableScope, TryExitableScope, SwitchExitableScope, LoopExitableScope } from './exitable-scope';

import { ABI } from './abi';
import { SRetABI } from './sret-abi';

let ir = llvm.IRBuilder;

let hasOwn = Object.prototype.hasOwnProperty;

class LLVMIRVisitor extends TreeVisitor {
    constructor (module, filename, options, abi, allModules, this_module_info, dibuilder, difile) {
        super();
        this.module = module;
        this.filename = filename;
        this.options = options;
        this.abi = abi;
        this.allModules = allModules;
        this.this_module_info = this_module_info;
        this.dibuilder = dibuilder;
        this.difile = difile;

        this.idgen = startGenerator();
        
        if (this.options.record_types)
            this.genRecordId = startGenerator();
        
        // build up our runtime method table
        this.ejs_intrinsics = Object.create(null, {
            templateDefaultHandlerCall: { value: this.handleTemplateDefaultHandlerCall },
            templateCallsite:           { value: this.handleTemplateCallsite },
            moduleGet:                  { value: this.handleModuleGet },
            moduleGetSlot:              { value: this.handleModuleGetSlot },
            moduleSetSlot:              { value: this.handleModuleSetSlot },
            moduleGetExotic:            { value: this.handleModuleGetExotic },
            getArgumentsObject:         { value: this.handleGetArgumentsObject },
            getLocal:                   { value: this.handleGetLocal },
            setLocal:                   { value: this.handleSetLocal },
            getGlobal:                  { value: this.handleGetGlobal },
            setGlobal:                  { value: this.handleSetGlobal },
            getArg:                     { value: this.handleGetArg },
            getNewTarget:               { value: this.handleGetNewTarget },
            slot:                       { value: this.handleGetSlot },
            setSlot:                    { value: this.handleSetSlot },
            invokeClosure:              { value: this.handleInvokeClosure },
            constructClosure:           { value: this.handleConstructClosure },
            constructSuper:             { value: this.handleConstructSuper },
            constructSuperApply:        { value: this.handleConstructSuperApply },
            setConstructorKindDerived:  { value: this.handleSetConstructorKindDerived },
            setConstructorKindBase:     { value: this.handleSetConstructorKindBase },
            makeClosure:                { value: this.handleMakeClosure },
            makeClosureNoEnv:           { value: this.handleMakeClosureNoEnv },
            makeAnonClosure:            { value: this.handleMakeAnonClosure },
            makeGenerator:              { value: this.handleMakeGenerator },
            generatorYield:             { value: this.handleGeneratorYield },
            createArgScratchArea:       { value: this.handleCreateArgScratchArea },
            makeClosureEnv:             { value: this.handleMakeClosureEnv },
            typeofIsObject:             { value: this.handleTypeofIsObject },
            typeofIsFunction:           { value: this.handleTypeofIsFunction },
            typeofIsString:             { value: this.handleTypeofIsString },
            typeofIsSymbol:             { value: this.handleTypeofIsSymbol },
            typeofIsNumber:             { value: this.handleTypeofIsNumber },
            typeofIsBoolean:            { value: this.handleTypeofIsBoolean },
            builtinUndefined:           { value: this.handleBuiltinUndefined },
            isNullOrUndefined:          { value: this.handleIsNullOrUndefined },
            isUndefined:                { value: this.handleIsUndefined },
            isNull:                     { value: this.handleIsNull },
            setPrototypeOf:             { value: this.handleSetPrototypeOf },
            objectCreate:               { value: this.handleObjectCreate },
            arrayFromRest:              { value: this.handleArrayFromRest },
            arrayFromSpread:            { value: this.handleArrayFromSpread },
            createIterResult:           { value: this.handleCreateIterResult },
            createIteratorWrapper:      { value: this.handleCreateIteratorWrapper }
        });

        this.opencode_intrinsics = {
            unaryNot          : true,

            templateDefaultHandlerCall: true,

            moduleGet         : true, // unused
            moduleGetSlot     : true,
            moduleSetSlot     : true,
            moduleGetExotic   : true,

            getLocal          : true, // unused
            setLocal          : true, // unused
            getGlobal         : true, // unused
            setGlobal         : true, // unused
            slot              : true,
            setSlot           : true,
            
            invokeClosure     : false,
            constructClosure  : false,
            makeClosure       : true,
            makeAnonClosure   : true,
            createArgScratchArea : true,
            makeClosureEnv    : true,
            setConstructorKindDerived: false,
            setConstructorKindBase: false,
            
            typeofIsObject    : true,
            typeofIsFunction  : true,
            typeofIsString    : true,
            typeofIsSymbol    : true,
            typeofIsNumber    : true,
            typeofIsBoolean   : true,
            builtinUndefined  : true,
            isNullOrUndefined : true, // unused
            isUndefined       : true,
            isNull            : true
        };
        
        this.llvm_intrinsics = {
            gcroot: () => module.getOrInsertIntrinsic("@llvm.gcroot")
        };

        this.ejs_runtime = runtime.createInterface(module, this.abi);
        this.ejs_binops = runtime.createBinopsInterface(module, this.abi);
        this.ejs_atoms = runtime.createAtomsInterface(module);
        this.ejs_globals = runtime.createGlobalsInterface(module);
        this.ejs_symbols = runtime.createSymbolsInterface(module);

        this.module_atoms = new Map();

        let init_function_name = `_ejs_module_init_string_literals_${this.filename}`;
        this.literalInitializationFunction = this.module.getOrInsertFunction(init_function_name, types.Void, []);

        if (this.options.debug)
            this.literalInitializationDebugInfo = this.dibuilder.createFunction(this.difile, init_function_name, init_function_name, this.difile, 0, false, true, 0, 0, true, this.literalInitializationFunction);
        
        // this function is only ever called by this module's toplevel
        this.literalInitializationFunction.setInternalLinkage();
        
        // initialize the scope stack with the global (empty) scope
        this.scope_stack = new Stack(new Map());

        let entry_bb = new llvm.BasicBlock("entry", this.literalInitializationFunction);
        let return_bb = new llvm.BasicBlock("return", this.literalInitializationFunction);

        if (this.options.debug)
            ir.setCurrentDebugLocation(llvm.DebugLoc.get(0, 0, this.literalInitializationDebugInfo));

        this.doInsideBBlock(entry_bb, () => { ir.createBr(return_bb); });
        this.doInsideBBlock(return_bb, () => {
            //this.createCall this.ejs_runtime.log, [consts.string(ir, "done with literal initialization")], ""
            ir.createRetVoid();
        });

        this.literalInitializationBB = entry_bb;
    }

    // lots of helper methods

    emitModuleInfo () {
        let this_module_type    = types.getModuleSpecificType(this.this_module_info.module_name, this.this_module_info.slot_num);
        this.this_module_global = new llvm.GlobalVariable(this.module, this_module_type, this.this_module_info.module_name, llvm.Constant.getAggregateZero(this_module_type), true);
        this.import_module_globals = new Map();
        for (let import_module_string of this.this_module_info.importList) {
            let import_module_info = this.allModules.get(import_module_string);
            if (!import_module_info.isNative())
                this.import_module_globals.set(import_module_string, new llvm.GlobalVariable(this.module, types.EjsModule, import_module_info.module_name, null, true));
        }
        this.this_module_initted = new llvm.GlobalVariable(this.module, types.Bool, `${this.this_module_info.module_name}_initialized`, consts.False(), false);
    }

    emitModuleResolution (module_accessors) {
        // this.loadUndefinedEjsValue depends on this
        this.currentFunction = this.toplevel_function;
                
        ir.setInsertPoint(this.resolve_modules_bb);
        if (this.options.debug)
            ir.setCurrentDebugLocation(llvm.DebugLoc.get(0, 0, this.currentFunction.debug_info));

        let uninitialized_bb = new llvm.BasicBlock("module_uninitialized", this.toplevel_function);
        let initialized_bb = new llvm.BasicBlock("module_initialized", this.toplevel_function);

        let load_init_flag = ir.createLoad(this.this_module_initted, "load_init_flag");
        let load_init_cmp = ir.createICmpEq(load_init_flag, consts.False(), "load_init_cmp");
                
        ir.createCondBr(load_init_cmp, uninitialized_bb, initialized_bb);
                
        ir.setInsertPoint(uninitialized_bb);
        ir.createStore(consts.True(), this.this_module_initted);

        ir.createCall(this.literalInitializationFunction, [], "");

        // fill in the information we know about this module
        //  our name
        let name_slot = ir.createInBoundsGetElementPointer(this.this_module_global, [consts.int32(0), consts.int32(1)], "name_slot");
        ir.createStore(consts.string(ir, this.this_module_info.path), name_slot);

        //  num_exports
        let num_exports_slot = ir.createInBoundsGetElementPointer(this.this_module_global, [consts.int32(0), consts.int32(2)], "num_exports_slot");
        ir.createStore(consts.int32(this.this_module_info.slot_num), num_exports_slot);

        // define our accessor properties
        for (let accessor of module_accessors) {
            let get_func = (accessor.getter && accessor.getter.ir_func) || consts.Null(types.EjsClosureFunc);
            let set_func = (accessor.setter && accessor.setter.ir_func) || consts.Null(types.EjsClosureFunc);
            let module_arg = ir.createPointerCast(this.this_module_global, types.EjsModule.pointerTo(), "");
            ir.createCall(this.ejs_runtime.module_add_export_accessors, [module_arg, consts.string(ir, accessor.key), get_func, set_func], "");
        };

        for (let import_module_string of this.this_module_info.importList) {
            let import_module = this.import_module_globals.get(import_module_string);
            if (import_module) {
                this.createCall(this.ejs_runtime.module_resolve, [import_module], "", !this.ejs_runtime.module_resolve.doesNotThrow);
            }
        }

        ir.createBr(this.toplevel_body_bb);

        ir.setInsertPoint(initialized_bb);
        return this.createRet(this.loadUndefinedEjsValue());
    }

    // result should be the landingpad's value
    beginCatch (result) { return this.createCall(this.ejs_runtime.begin_catch, [ir.createPointerCast(result, types.Int8Pointer, "")], "begincatch"); }
    endCatch   ()       { return this.createCall(this.ejs_runtime.end_catch, [], "endcatch"); }

    doInsideExitableScope (scope, f) {
        scope.enter();
        f();
        scope.leave();
    }
    
    doInsideBBlock (b, f) {
        let saved = ir.getInsertBlock();
        ir.setInsertPoint(b);
        f();
        ir.setInsertPoint(saved);
        return b;
    }
    
    createLoad (value, name) {
        let rv = ir.createLoad(value, name);
        rv.setAlignment(8);
        return rv;
    }

    loadCachedEjsValue (name, init) {
        let alloca_name = `${name}_alloca`;
        let load_name = `${name}_load`;

        let alloca;
        if (this.currentFunction[alloca_name]) {
            alloca = this.currentFunction[alloca_name];
        }
        else {
            alloca = this.createAlloca(this.currentFunction, types.EjsValue, alloca_name);
            this.currentFunction[alloca_name] = alloca;
            this.doInsideBBlock(this.currentFunction.entry_bb, () => init(alloca));
        }

        return ir.createLoad(alloca, load_name);
    }

    loadBoolEjsValue (n) {
        let rv = this.loadCachedEjsValue(n, (alloca) => {
                let alloca_as_int64 = ir.createBitCast(alloca, types.Int64.pointerTo(), "alloca_as_pointer");
                if (this.options.target_pointer_size === 64)
                    ir.createStore(consts.int64_lowhi(0xfff98000,  n ? 0x00000001 : 0x000000000), alloca_as_int64);
                else
                    ir.createStore(consts.int64_lowhi(0xffffff83,  n ? 0x00000001 : 0x000000000), alloca_as_int64);
        });
        rv._ejs_returns_ejsval_bool = true;
        return rv;
    }

    loadDoubleEjsValue (n)   { return this.loadCachedEjsValue(`num_${n}`, (alloca) => this.storeDouble(alloca, n)); }
    loadNullEjsValue ()      { return this.loadCachedEjsValue("null",     (alloca) => this.storeNull(alloca));      }
    loadUndefinedEjsValue () { return this.loadCachedEjsValue("undef",    (alloca) => this.storeUndefined(alloca)); }

    storeUndefined (alloca, name) {
        let alloca_as_int64 = ir.createBitCast(alloca, types.Int64.pointerTo(), "alloca_as_pointer");
        if (this.options.target_pointer_size === 64)
            return ir.createStore(consts.int64_lowhi(0xfff90000, 0x00000000), alloca_as_int64, name);
        else // 32 bit
            return ir.createStore(consts.int64_lowhi(0xffffff82, 0x00000000), alloca_as_int64, name);
    }

    storeNull (alloca, name) {
        let alloca_as_int64 = ir.createBitCast(alloca, types.Int64.pointerTo(), "alloca_as_pointer");
        if (this.options.target_pointer_size === 64)
            return ir.createStore(consts.int64_lowhi(0xfffb8000, 0x00000000), alloca_as_int64, name);
        else // 32 bit
            return ir.createStore(consts.int64_lowhi(0xffffff87, 0x00000000), alloca_as_int64, name);
    }

    storeDouble (alloca, jsnum, name) {
        let c = llvm.ConstantFP.getDouble(jsnum);
        let alloca_as_double = ir.createBitCast(alloca, types.Double.pointerTo(), "alloca_as_pointer");
        return ir.createStore(c, alloca_as_double, name);
    }

    storeBoolean (alloca, jsbool, name) {
        let alloca_as_int64 = ir.createBitCast(alloca, types.Int64.pointerTo(), "alloca_as_pointer");
        if (this.options.target_pointer_size === 64)
            return ir.createStore(consts.int64_lowhi(0xfff98000, jsbool ? 0x00000001 : 0x000000000), alloca_as_int64, name);
        else
            return ir.createStore(consts.int64_lowhi(0xffffff83, jsbool ? 0x00000001 : 0x000000000), alloca_as_int64, name);
    }

    storeToDest (dest, arg, name = "") {
        if (!arg)
            arg = { type: b.Literal, value: null };

        if (arg.type === b.Literal) {
            if (arg.value === null)
                return this.storeNull(dest, name);

            if (arg.value === undefined)
                return this.storeUndefined(dest, name);

            if (typeof arg.value === "number")
                return this.storeDouble(dest, arg.value, name);

            if (typeof arg.value === "boolean")
                return this.storeBoolean(dest, arg.value, name);

            // if typeof arg is "string"
            let val = this.visit(arg);
            return ir.createStore(val, dest, name);
        }
        else {
            let val = this.visit(arg);
            return ir.createStore(val, dest, name);
        }
    }
    
    storeGlobal (prop, value) {
        let gname;
        // we store obj.prop, prop is an id
        if (prop.type === b.Identifier)
            gname = prop.name;
        else // prop.type is b.Literal
            gname = prop.value;

        let c = this.getAtom(gname);

        debug.log( () => `createPropertyStore %global[${gname}]` );
        
        return this.createCall(this.ejs_runtime.global_setprop, [c, value], `globalpropstore_${gname}`);
    }

    loadGlobal (prop) {
        let gname = prop.name;

        if (this.options.frozen_global)
            return ir.createLoad(this.ejs_globals[prop.name], `load-${gname}`);

        let pname = this.getAtom(gname);
        return this.createCall(this.ejs_runtime.global_getprop, [pname], `globalloadprop_${gname}`);
    }

    visitWithScope (scope, children) {
        this.scope_stack.push(scope);
        for (let child of children)
            this.visit(child);
        this.scope_stack.pop();
    }

    findIdentifierInScope (ident) {
        for (let scope of this.scope_stack.stack) {
            if (scope.has(ident))
                return scope.get(ident);
        }
        return null;
    }

    
    createAlloca (func, type, name) {
        let saved_insert_point = ir.getInsertBlock();
        ir.setInsertPointStartBB(func.entry_bb);
        let alloca = ir.createAlloca(type, name);

        // if EjsValue was a pointer value we would be able to use an the llvm gcroot intrinsic here.  but with the nan boxing
        // we kinda lose out as the llvm IR code doesn't permit non-reference types to be gc roots.
        // if type is types.EjsValue
        //        // EjsValues are rooted
        //        this.createCall this.llvm_intrinsics.gcroot(), [(ir.createPointerCast alloca, types.Int8Pointer.pointerTo(), "rooted_alloca"), consts.Null types.Int8Pointer], ""

        ir.setInsertPoint(saved_insert_point);
        return alloca;
    }

    createAllocas (func, ids, scope) {
        let allocas = [];
        let new_allocas = [];
        
        // the allocas are always allocated in the function entry_bb so the mem2reg opt pass can regenerate the ssa form for us
        let saved_insert_point = ir.getInsertBlock();
        ir.setInsertPointStartBB(func.entry_bb);

        let j = 0;
        for (let i = 0, e = ids.length; i < e; i ++) {
            let name = ids[i].id.name;
            if (!scope.has(name)) {
                allocas[j] = ir.createAlloca(types.EjsValue, `local_${name}`);
                allocas[j].setAlignment(8);
                scope.set(name, allocas[j]);
                new_allocas[j] = true;
            }
            else {
                allocas[j] = scope.get(name);
                new_allocas[j] = false;
            }
            j = j + 1;
        }
        

        // reinstate the IRBuilder to its previous insert point so we can insert the actual initializations
        ir.setInsertPoint(saved_insert_point);

        return { allocas: allocas, new_allocas: new_allocas };
    }

    createPropertyStore (obj,prop,rhs,computed) {
        if (computed) {
            // we store obj[prop], prop can be any value
            return this.createCall(this.ejs_runtime.object_setprop, [obj, this.visit(prop), rhs], "propstore_computed");
        }
        else {
            var pname;

            // we store obj.prop, prop is an id
            if (prop.type === b.Identifier)
                pname = prop.name;
            else // prop.type is b.Literal
                pname = prop.value;

            let c = this.getAtom(pname);

            debug.log(() => `createPropertyStore ${obj}[${pname}]`);

            return this.createCall(this.ejs_runtime.object_setprop, [obj, c, rhs], `propstore_${pname}`);
        }
    }
    
    createPropertyLoad (obj,prop,computed,canThrow = true) {
        if (computed) {
            // we load obj[prop], prop can be any value
            let loadprop = this.visit(prop);
            
            if (this.options.record_types)
                this.createCall(this.ejs_runtime.record_getprop, [consts.int32(this.genRecordId()), obj, loadprop], "");
            
            return this.createCall(this.ejs_runtime.object_getprop, [obj, loadprop], "getprop_computed", canThrow);
        }
        else {
            // we load obj.prop, prop is an id
            let pname = this.getAtom(prop.name);

            if (this.options.record_types)
                this.createCall(this.ejs_runtime.record_getprop, [consts.int32(this.genRecordId()), obj, pname], "");

            return this.createCall(this.ejs_runtime.object_getprop, [obj, pname], `getprop_${prop.name}`, canThrow);
        }
    }

    setDebugLoc (ast_node) {
        if (!this.options.debug) return;
        if (!ast_node || !ast_node.loc) return;
        if (!this.currentFunction) return;
        if (!this.currentFunction.debug_info) return;

        ir.setCurrentDebugLocation(llvm.DebugLoc.get(ast_node.loc.start.line, ast_node.loc.start.column, this.currentFunction.debug_info));
    }
    
    visit (n) {
        this.setDebugLoc(n);
        return super.visit(n);
    }

    visitOrNull      (n) { return this.visit(n) || this.loadNullEjsValue(); }
    visitOrUndefined (n) { return this.visit(n) || this.loadUndefinedEjsValue(); }
    
    visitProgram (n) {
        // by the time we make it here the program has been
        // transformed so that there is nothing at the toplevel
        // but function declarations.
        for (let func of n.body)
            this.visit(func);
    }

    visitBlock (n) {
        let new_scope = new Map();

        let iife_dest_bb = null;
        let iife_rv = null;
        
        if (n.fromIIFE) {
            let insertBlock = ir.getInsertBlock();
            let insertFunc = insertBlock.parent;

            iife_dest_bb = new llvm.BasicBlock("iife_dest", insertFunc);
            iife_rv = n.ejs_iife_rv;
        }

        this.iifeStack.push ({ iife_rv, iife_dest_bb });

        this.visitWithScope(new_scope, n.body);

        this.iifeStack.pop();
        if (iife_dest_bb) {
            ir.createBr(iife_dest_bb);
            ir.setInsertPoint(iife_dest_bb);
            let rv = this.createLoad(this.findIdentifierInScope(iife_rv.name), "%iife_rv_load");
            return rv;
        }
        else {
            return n;
        }
    }

    visitSwitch (n) {
        let insertBlock = ir.getInsertBlock();
        let insertFunc = insertBlock.parent;

        // find the default: case first
        let defaultCase = null;
        for (let _case of n.cases) {
            if (!_case.test) {
                defaultCase = _case;
                break;
            }
        }

        // for each case, create 2 basic blocks
        for (let _case of n.cases) {
            _case.bb = new llvm.BasicBlock("case_bb", insertFunc);
            if (_case !== defaultCase)
                _case.dest_check = new llvm.BasicBlock("case_dest_check_bb", insertFunc);
        }

        let merge_bb = new llvm.BasicBlock("switch_merge", insertFunc);

        let discr = this.visit(n.discriminant);

        let case_checks = [];
        for (let _case of n.cases) {
            if (defaultCase !== _case)
                case_checks.push ({ test: _case.test, dest_check: _case.dest_check, body: _case.bb });
        }

        case_checks.push ({ dest_check: defaultCase ? defaultCase.bb : merge_bb });

        this.doInsideExitableScope (new SwitchExitableScope(merge_bb), () => {

            // insert all the code for the tests
            ir.createBr(case_checks[0].dest_check);
            ir.setInsertPoint(case_checks[0].dest_check);
            for (let casenum = 0; casenum < case_checks.length -1; casenum ++) {
                let test = this.visit(case_checks[casenum].test);
                let eqop = this.ejs_binops["==="];

                this.setDebugLoc(test);
                let discTest = this.createCall(eqop, [discr, test], "test", !eqop.doesNotThrow);
                
                let disc_cmp, disc_truthy;

                if (discTest._ejs_returns_ejsval_bool) {
                    disc_cmp = this.createEjsvalICmpEq(discTest, consts.ejsval_false());
                }
                else {
                    disc_truthy = this.createCall(this.ejs_runtime.truthy, [discTest], "disc_truthy");
                    disc_cmp = ir.createICmpEq(disc_truthy, consts.False(), "disccmpresult");
                }
                ir.createCondBr(disc_cmp, case_checks[casenum+1].dest_check, case_checks[casenum].body);
                ir.setInsertPoint(case_checks[casenum+1].dest_check);
            }


            let case_bodies = [];
            
            // now insert all the code for the case consequents
            for (let _case of n.cases)
                case_bodies.push ({bb:_case.bb, consequent:_case.consequent});

            case_bodies.push ({bb:merge_bb});

            for (let casenum = 0; casenum < case_bodies.length-1; casenum ++) {
                ir.setInsertPoint(case_bodies[casenum].bb);
                case_bodies[casenum].consequent.forEach ((consequent, i) => {
                    this.visit(consequent);
                });
                
                ir.createBr(case_bodies[casenum+1].bb);
            }
            
            ir.setInsertPoint(merge_bb);
        });

        return merge_bb;
    }
    
    visitCase (n)  {
        throw new Error("we shouldn't get here, case statements are handled in visitSwitch");
    }
    
    visitLabeledStatement (n) {
        n.body.label = n.label.name;
        return this.visit(n.body);
    }

    visitBreak (n) {
        return ExitableScope.scopeStack.exitAft(true, n.label && n.label.name);
    }

    visitContinue (n) {
        if (n.label && n.label.name)
            return LoopExitableScope.findLabeledOrFinally(n.label.name).exitFore();
        else
            return LoopExitableScope.findLoopOrFinally().exitFore();
    }

    generateCondBr (exp, then_bb, else_bb) {
        let cmp, exp_value;
        if (exp.type === b.Literal && typeof exp.value === "boolean") {
            cmp = consts.int1(exp.value ? 0 : 1); // we check for false below, so the then/else branches get swapped
        }
        else {
            exp_value = this.visit(exp);
            if (exp_value._ejs_returns_ejsval_bool) {
                cmp = this.createEjsvalICmpEq(exp_value, consts.ejsval_false(), "cmpresult");
            }
            else if (exp_value._ejs_returns_native_bool) {
                cmp = ir.createSelect(exp_value, consts.int1(0), consts.int1(1), "invert_check");
            }
            else {
                let cond_truthy = this.createCall(this.ejs_runtime.truthy, [exp_value], "cond_truthy");
                cmp = ir.createICmpEq(cond_truthy, consts.False(), "cmpresult");
            }
        }
        ir.createCondBr(cmp, else_bb, then_bb);
        return exp_value;
    }

    
    visitFor (n) {
        let insertBlock = ir.getInsertBlock();
        let insertFunc = insertBlock.parent;

        let init_bb   = new llvm.BasicBlock("for_init",   insertFunc);
        let test_bb   = new llvm.BasicBlock("for_test",   insertFunc);
        let body_bb   = new llvm.BasicBlock("for_body",   insertFunc);
        let update_bb = new llvm.BasicBlock("for_update", insertFunc);
        let merge_bb  = new llvm.BasicBlock("for_merge",  insertFunc);

        ir.createBr(init_bb);

        this.doInsideBBlock(init_bb, () => {
            this.visit(n.init);
            ir.createBr(test_bb);
        });

        this.doInsideBBlock(test_bb, () => {
            if (n.test)
                this.generateCondBr(n.test, body_bb, merge_bb);
            else
                ir.createBr(body_bb);
        });

        this.doInsideExitableScope (new LoopExitableScope(n.label, update_bb, merge_bb), () => {
            this.doInsideBBlock(body_bb, () => {
                this.visit(n.body);
                ir.createBr(update_bb);
            });

            this.doInsideBBlock(update_bb, () => {
                this.visit(n.update);
                ir.createBr(test_bb);
            });
        });

        ir.setInsertPoint(merge_bb);
        return merge_bb;
    }

    visitDo (n) {
        let insertBlock = ir.getInsertBlock();
        let insertFunc = insertBlock.parent;
        
        let body_bb = new llvm.BasicBlock("do_body", insertFunc);
        let test_bb = new llvm.BasicBlock("do_test", insertFunc);
        let merge_bb = new llvm.BasicBlock("do_merge", insertFunc);

        ir.createBr(body_bb);

        this.doInsideExitableScope (new LoopExitableScope(n.label, test_bb, merge_bb), () => {
            this.doInsideBBlock(body_bb, () => {
                this.visit(n.body);
                ir.createBr(test_bb);
            });
            this.doInsideBBlock(test_bb, () => {
                this.generateCondBr(n.test, body_bb, merge_bb);
            });
        });
        
        ir.setInsertPoint(merge_bb);
        return merge_bb;
    }

    
    visitWhile (n) {
        let insertBlock = ir.getInsertBlock();
        let insertFunc = insertBlock.parent;
        
        let while_bb  = new llvm.BasicBlock("while_start", insertFunc);
        let body_bb = new llvm.BasicBlock("while_body", insertFunc);
        let merge_bb = new llvm.BasicBlock("while_merge", insertFunc);

        ir.createBr(while_bb);

        this.doInsideBBlock(while_bb, () => {
            this.generateCondBr(n.test, body_bb, merge_bb);
        });

        this.doInsideExitableScope (new LoopExitableScope(n.label, while_bb, merge_bb), () => {
            this.doInsideBBlock(body_bb, () => {
                this.visit(n.body);
                ir.createBr(while_bb);
            });
        });
        
        ir.setInsertPoint(merge_bb);
        return merge_bb;
    }

    visitForIn (n) {
        let insertBlock = ir.getInsertBlock();
        let insertFunc = insertBlock.parent;

        let iterator = this.createCall(this.ejs_runtime.prop_iterator_new, [this.visit(n.right)], "iterator");

        let lhs;
        // make sure we get an alloca if there's a "var"
        if (n.left[0]) {
            this.visit(n.left);
            lhs = n.left[0].declarations[0].id;
        }
        else {
            lhs = n.left;
        }
        
        let forin_bb  = new llvm.BasicBlock ("forin_start", insertFunc);
        let body_bb   = new llvm.BasicBlock ("forin_body",  insertFunc);
        let merge_bb  = new llvm.BasicBlock ("forin_merge", insertFunc);
        
        ir.createBr(forin_bb);

        this.doInsideExitableScope (new LoopExitableScope (n.label, forin_bb, merge_bb), () => {
            // forin_bb:
            //     moreleft = prop_iterator_next (iterator, true)
            //     if moreleft === false
            //         goto merge_bb
            //     else
            //         goto body_bb
            // 
            this.doInsideBBlock (forin_bb, () => {
                let moreleft = this.createCall(this.ejs_runtime.prop_iterator_next, [iterator, consts.True()], "moreleft");
                let cmp = ir.createICmpEq(moreleft, consts.False(), "cmpmoreleft");
                ir.createCondBr(cmp, merge_bb, body_bb);
            });

            // body_bb:
            //     current = prop_iteratorcurrent (iterator)
            //     *lhs = current
            //      <body>
            //     goto forin_bb
            this.doInsideBBlock (body_bb, () => {
                let current = this.createCall(this.ejs_runtime.prop_iterator_current, [iterator], "iterator_current");
                this.storeValueInDest(current, lhs);
                this.visit(n.body);
                ir.createBr(forin_bb);
            });
        });

        // merge_bb:
        // 
        ir.setInsertPoint(merge_bb);
        return merge_bb;
    }

    visitForOf (n) {
        throw new Error("internal compiler error.  for..of statements should have been transformed away by this point.");
    }
    
    visitUpdateExpression (n) {
        let result = this.createAlloca(this.currentFunction, types.EjsValue, "%update_result");
        let argument = this.visit(n.argument);
        
        let one = this.loadDoubleEjsValue(1);
        
        if (!n.prefix) {
            // postfix updates store the argument before the op
            ir.createStore(argument, result);
        }

        // argument = argument $op 1
        let update_op = this.ejs_binops[n.operator === '++' ? '+' : '-'];
        let temp = this.createCall(update_op, [argument, one], "update_temp", !update_op.doesNotThrow);
        
        this.storeValueInDest(temp, n.argument);
        
        // return result
        if (n.prefix) {
            argument = this.visit(n.argument);
            // prefix updates store the argument after the op
            ir.createStore(argument, result);
        }
        return this.createLoad(result, "%update_result_load");
    }

    visitConditionalExpression (n) {
        return this.visitIfOrCondExp(n, true);
    }
    
    visitIf (n) {
        return this.visitIfOrCondExp(n, false);
    }

    visitIfOrCondExp (n, load_result) {
        let cond_val;

        if (load_result)
            cond_val = this.createAlloca(this.currentFunction, types.EjsValue, "%cond_val");
        
        let insertBlock = ir.getInsertBlock();
        let insertFunc = insertBlock.parent;

        let then_bb  = new llvm.BasicBlock ("then", insertFunc);
        let else_bb;
        if (n.alternate)
            else_bb = new llvm.BasicBlock ("else", insertFunc);
        let merge_bb = new llvm.BasicBlock ("merge", insertFunc);

        this.generateCondBr(n.test, then_bb, else_bb ? else_bb : merge_bb);

        this.doInsideBBlock(then_bb, () => {
            let then_val = this.visit(n.consequent);
            if (load_result) ir.createStore(then_val, cond_val);
            ir.createBr(merge_bb);
        });

        if (n.alternate) {
            this.doInsideBBlock(else_bb, () => {
                let else_val = this.visit(n.alternate);
                if (load_result) ir.createStore(else_val, cond_val);
                ir.createBr(merge_bb);
            });
        }

        ir.setInsertPoint(merge_bb);
        if (load_result)
            return this.createLoad(cond_val, "cond_val_load");
        else
            return merge_bb;
    }
    
    visitReturn (n) {
        if (this.iifeStack.top.iife_rv) {
            // if we're inside an IIFE, convert the return statement into a store to the iife_rv alloca + a branch to the iife's dest bb
            if (n.argument)
                ir.createStore(this.visit(n.argument), this.findIdentifierInScope(this.iifeStack.top.iife_rv.name));
            ir.createBr(this.iifeStack.top.iife_dest_bb);
        }
        else {
            // otherwise generate an llvm IR ret
            let rv = this.visitOrUndefined(n.argument);

            if (this.finallyStack.length > 0) {
                if (!this.currentFunction.returnValueAlloca)
                    this.currentFunction.returnValueAlloca = this.createAlloca(this.currentFunction, types.EjsValue, "returnValue");
                ir.createStore(rv, this.currentFunction.returnValueAlloca);
                ir.createStore(consts.int32(ExitableScope.REASON_RETURN), this.currentFunction.cleanup_reason);
                ir.createBr(this.finallyStack[0]);
            }
            else {
                let return_alloca = this.createAlloca(this.currentFunction, types.EjsValue, "return_alloca");
                ir.createStore(rv, return_alloca);

                this.createRet(this.createLoad(return_alloca, "return_load"));
            }
        }
    }
    
    visitVariableDeclaration (n) {
        if (n.kind === "var") throw new Error("internal compiler error.  'var' declarations should have been transformed to 'let's by this point.");
        
        let scope = this.scope_stack.top;

        let { allocas, new_allocas } = this.createAllocas(this.currentFunction, n.declarations, scope);
        for (let i = 0, e = n.declarations.length; i < e; i ++) {
            if (!n.declarations[i].init) {
                // there was not an initializer. we only store undefined
                // if the alloca is newly allocated.
                if (new_allocas[i]) {
                    let initializer = this.visitOrUndefined(n.declarations[i].init);
                    ir.createStore(initializer, allocas[i]);
                }
            }
            else {
                let initializer = this.visitOrUndefined(n.declarations[i].init);
                ir.createStore(initializer, allocas[i]);
            }
        }
    }

    visitMemberExpression (n) {
        return this.createPropertyLoad(this.visit(n.object), n.property, n.computed);
    }

    storeValueInDest (rhvalue, lhs) {
        if (lhs.type === b.Identifier) {
            let dest = this.findIdentifierInScope(lhs.name);
            let result;
            if (dest)
                result = ir.createStore(rhvalue, dest);
            else
                result = this.storeGlobal(lhs, rhvalue);
            return result;
        }
        else if (lhs.type === b.MemberExpression) {
            return this.createPropertyStore(this.visit(lhs.object), lhs.property, rhvalue, lhs.computed);
        }
        else if (is_intrinsic(lhs, "%slot")) {
            return ir.createStore(rhvalue, this.handleSlotRef(lhs));
        }
        else if (is_intrinsic(lhs, "%getLocal")) {
            return ir.createStore(rhvalue, this.findIdentifierInScope(lhs.arguments[0].name));
        }
        else if (is_intrinsic(lhs, "%getGlobal")) {
            let gname = lhs.arguments[0].name;

            return this.createCall(this.ejs_runtime.global_setprop, [this.getAtom(gname), rhvalue], `globalpropstore_${lhs.arguments[0].name}`);
        }
        else {
            throw new Error(`unhandled lhs ${escodegenerate(lhs)}`);
        }
    }

    visitAssignmentExpression (n) {
        let lhs = n.left;
        let rhs = n.right;

        let rhvalue = this.visit(rhs);

        if (n.operator.length === 2)
            throw new Error(`binary assignment operators '${n.operator}' should not exist at this point`);

        if (this.options.record_types)
            this.createCall(this.ejs_runtime.record_assignment, [consts.int32(this.genRecordId()), rhvalue], "");
        this.storeValueInDest(rhvalue, lhs);

        // we need to visit lhs after the store so that we load the value, but only if it's used
        if (!n.result_not_used)
            return rhvalue;
    }

    visitFunction (n) {
        if (!n.toplevel)
            debug.log (() => `        function ${n.ir_name} at ${this.filename}:${n.loc ? n.loc.start.line : '<unknown>'}`);
        
        // save off the insert point so we can get back to it after generating this function
        let insertBlock = ir.getInsertBlock();

        for (let param of n.formal_params) {
            if (param.type !== b.Identifier)
                throw new Error("formal parameters should only be identifiers by this point");
        }

        // XXX this methods needs to be augmented so that we can pass actual types (or the builtin args need
        // to be reflected in jsllvm.cpp too).  maybe we can pass the names to this method and it can do it all
        // there?

        let ir_func = n.ir_func;
        let ir_args = n.ir_func.args;
        debug.log ("");
        //debug.log -> `ir_func = ${ir_func}`

        //debug.log -> `param ${param.llvm_type} ${param.name}` for param in n.formal_params

        this.currentFunction = ir_func;

        // we need to do this here as well, since otherwise the allocas and stores we create below for our parameters
        // could be accidentally attributed to the previous @currentFunction (the last location we set).
        this.setDebugLoc(n);

        // Create a new basic block to start insertion into.
        let entry_bb = new llvm.BasicBlock("entry", ir_func);

        ir.setInsertPoint(entry_bb);

        let new_scope = new Map();

        // we save off the top scope and entry_bb of the function so that we can hoist vars there
        ir_func.topScope = new_scope;
        ir_func.entry_bb = entry_bb;

        ir_func.literalAllocas = Object.create(null);

        let allocas = [];

        // create allocas for the builtin args
        for (let param of n.params) {
            let alloca = ir.createAlloca(param.llvm_type, `local_${param.name}`);
            alloca.setAlignment(8);
            new_scope.set(param.name, alloca);
            allocas.push(alloca);
        }
        
        // now create allocas for the formal parameters
        let first_formal_index = allocas.length;
        for (let param of n.formal_params) {
            let alloca = this.createAlloca(this.currentFunction, types.EjsValue, `local_${param.name}`);
            new_scope.set(param.name, alloca);
            allocas.push(alloca);
        }

        debug.log ( () => {
            allocas.map( (alloca) => `alloca ${alloca}`).join('\n');
        });
        
        // now store the arguments onto the stack
        for (let i = 0, e = n.params.length; i < e; i ++) {
            var store = ir.createStore(ir_args[i], allocas[i]);
            debug.log ( () => `store ${store} *builtin` );
        }

        let body_bb = new llvm.BasicBlock("body", ir_func);
        ir.setInsertPoint(body_bb);

        //this.createCall this.ejs_runtime.log, [consts.string(ir, `entering ${n.ir_name}`)], ""
        
        let insertFunc = body_bb.parent;
        
        this.iifeStack = new Stack();

        this.finallyStack = [];
        
        this.visitWithScope(new_scope, [n.body]);

        // XXX more needed here - this lacks all sorts of control flow stuff.
        // Finish off the function.
        this.createRet(this.loadUndefinedEjsValue());

        if (n.toplevel) {
            this.resolve_modules_bb = new llvm.BasicBlock("resolve_modules", ir_func);
            this.toplevel_body_bb = body_bb;
            this.toplevel_function = ir_func;

            // branch to the resolve_modules_bb from our entry_bb, but only in the toplevel function
            ir.setInsertPoint(entry_bb);
            ir.createBr(this.resolve_modules_bb);
        }
        else {
            // branch to the body_bb from our entry_bb
            ir.setInsertPoint(entry_bb);
            ir.createBr(body_bb);
        }

        this.currentFunction = null;

        ir.setInsertPoint(insertBlock);

        return ir_func;
    }

    createRet (x) {
        //this.createCall this.ejs_runtime.log, [consts.string(ir, `leaving ${this.currentFunction.name}`)], ""
        return this.abi.createRet(this.currentFunction, x);
    }
    
    visitUnaryExpression (n) {
        debug.log ( () => `operator = '${n.operator}'` );

        let builtin = `unop${n.operator}`;
        let callee = this.ejs_runtime[builtin];
        
        if (n.operator === "delete") {
            if (n.argument.type !== b.MemberExpression) throw "unhandled delete syntax";
            
            let fake_literal = {
                type: b.Literal,
                value: n.argument.property.name,
                raw: `'${n.argument.property.name}'`
            };
            return this.createCall(callee, [this.visitOrNull(n.argument.object), this.visit(fake_literal)], "result");
        }
        else if (n.operator === "!") {
            let arg_value =  this.visitOrNull(n.argument);
            if (this.opencode_intrinsics.unaryNot && this.options.target_pointer_size === 64 && arg_value._ejs_returns_ejsval_bool) {
                let cmp = this.createEjsvalICmpEq(arg_value, consts.ejsval_true(), "cmpresult");
                return this.createEjsBoolSelect(cmp, true);
            }
            else {
                return this.createCall(callee, [arg_value], "result");
            }
        }
        else {
            if (!callee) {
                throw new Error(`Internal error: unary operator '${n.operator}' not implemented`);
            }
            return this.createCall(callee, [this.visitOrNull(n.argument)], "result");
        }
    }
    

    visitSequenceExpression (n) {
        let rv = null;
        for (let exp of n.expressions)
            rv = this.visit(exp);
        return rv;
    }
    
    visitBinaryExpression (n) {
        debug.log ( () => `operator = '${n.operator}'` );
        let callee = this.ejs_binops[n.operator];

        if (!callee)
            throw new Error(`Internal error: unhandled binary operator '${n.operator}'`);

        let left_visited = this.visit(n.left);
        let right_visited = this.visit(n.right);

        if (this.options.record_types)
            this.createCall(this.ejs_runtime.record_binop, [consts.int32(this.genRecordId()), consts.string(ir, n.operator), left_visited, right_visited], "");

        // call the actual runtime binaryop method
        return this.createCall(callee, [left_visited, right_visited], `result_${n.operator}`, !callee.doesNotThrow);
    }
    
    visitLogicalExpression (n) {
        debug.log ( () => `operator = '${n.operator}'` );
        let result = this.createAlloca(this.currentFunction, types.EjsValue, `result_${n.operator}`);

        let insertBlock = ir.getInsertBlock();
        let insertFunc = insertBlock.parent;
        
        let left_bb  = new llvm.BasicBlock  ("cond_left", insertFunc);
        let right_bb  = new llvm.BasicBlock ("cond_right", insertFunc);
        let merge_bb = new llvm.BasicBlock  ("cond_merge", insertFunc);

        // we invert the test here - check if the condition is false/0
        let left_visited = this.generateCondBr(n.left, left_bb, right_bb);

        this.doInsideBBlock(left_bb, () => {
            // inside the else branch, left was truthy
            if (n.operator === "||")
                // for || we short circuit out here
                ir.createStore(left_visited, result);
            else if (n.operator === "&&")
                // for && we evaluate the second and store it
                ir.createStore(this.visit(n.right), result);
            else
                throw "Internal error 99.1";
            ir.createBr(merge_bb);
        });

        this.doInsideBBlock(right_bb, () => {
            // inside the then branch, left was falsy
            if (n.operator === "||")
                // for || we evaluate the second and store it
                ir.createStore(this.visit(n.right), result);
            else if (n.operator === "&&")
                // for && we short circuit out here
                ir.createStore(left_visited, result);
            else
                throw "Internal error 99.1";
            ir.createBr(merge_bb);
        });

        ir.setInsertPoint(merge_bb);
        return this.createLoad(result, `result_${n.operator}_load`);
    }

    visitArgsForCall (callee, pullThisFromArg0, args) {
        args = args.slice();
        let argv = [];

        if (callee.takes_builtins) {
            let thisArg, closure;
            if (pullThisFromArg0 && args[0].type === b.MemberExpression) {
                thisArg = this.visit(args[0].object);
                closure = this.createPropertyLoad(thisArg, args[0].property, args[0].computed);
            }
            else {
                thisArg = this.loadUndefinedEjsValue();
                closure = this.visit(args[0]);
            }

            let this_alloca = this.createAlloca(this.currentFunction, types.EjsValue, "this_alloca");
            ir.createStore(thisArg, this_alloca, "this_alloca_store");

            args.shift();
            
            argv.push(closure);                    // %closure
            argv.push(this_alloca);                // %this
            argv.push(consts.int32(args.length));  // %argc

            let args_length = args.length;
            if (args_length > 0) {
                for (let i = 0; i < args_length; i ++) {
                    args[i] = this.visitOrNull(args[i]);
                }
                for (let i = 0; i < args_length; i ++) {
                    let gep = ir.createGetElementPointer(this.currentFunction.scratch_area, [consts.int32(0), consts.int64(i)], `arg_gep_${i}`);
                    ir.createStore(args[i], gep, `argv[${i}]-store`);
                }

                let argsCast = ir.createGetElementPointer(this.currentFunction.scratch_area, [consts.int32(0), consts.int64(0)], "call_args_load");

                argv.push(argsCast);
            }
            else {
                argv.push(consts.Null(types.EjsValue.pointerTo()));
            }

            argv.push(this.loadUndefinedEjsValue()); // %newTarget = undefined
        }               
        else {
            for (let a of args)
                argv.push(this.visitOrNull(a));
        }

        return argv;
    }

    debugLog (str) {
        if (this.options.debug_level > 0)
            this.createCall(this.ejs_runtime.log, [consts.string(ir, str)], "");
    }

    visitArgsForConstruct (callee, args, this_loc, newTarget_loc) {
        let insertBlock = ir.getInsertBlock();
        let insertFunc  = insertBlock.parent;
        
        args = args.slice();
        let argv = [];
        // constructors are always .takes_builtins, so we can skip the other case
        // 

        let ctor = this.visit(args[0]);
        args.shift();

        argv.push(ctor);                          // %closure
        argv.push(this_loc);                      // %this
        argv.push(consts.int32(args.length));     // %argc

        if (args.length > 0) {
            let visited = [];
            for (let a of args)
                visited.push(this.visitOrNull(a));

            visited.forEach ((a,i) => {
                let gep = ir.createGetElementPointer(this.currentFunction.scratch_area, [consts.int32(0), consts.int64(i)], `arg_gep_${i}`);
                ir.createStore(a, gep, `argv[${i}]-store`);
            });

            let argsCast = ir.createGetElementPointer(this.currentFunction.scratch_area, [consts.int32(0), consts.int64(0)], "call_args_load");
            argv.push(argsCast);
        }
        else {
            argv.push(consts.Null(types.EjsValue.pointerTo()));
        }

        argv.push(newTarget_loc || ctor); // %newTarget = ctor

        return argv;
    }
    
    visitCallExpression (n) {
        debug.log ( () => `visitCall ${JSON.stringify(n)}` );
        debug.log ( () => `          arguments length = ${n.arguments.length}`);

        debug.log ( () => {
            return n.arguments.map ( (a, i) => `          arguments[${i}] =  ${JSON.stringify(a)}` ).join('');
        });

        let unescapedName = n.callee.name.slice(1);
        let intrinsicHandler = this.ejs_intrinsics[unescapedName];
        if (!intrinsicHandler)
            throw new Error(`Internal error: callee should not be null in visitCallExpression (callee = '${n.callee.name}', arguments = ${n.arguments.length})`);

        return intrinsicHandler.call(this, n, this.opencode_intrinsics[unescapedName]);
    }
    
    visitThisExpression (n) {
        debug.log("visitThisExpression");
        return this.createLoad(this.createLoad(this.findIdentifierInScope("%this"), "load_this_ptr"), "load_this");
    }

    visitSpreadElement (n) {
        throw new Error("halp");
    }
    
    visitIdentifier (n) {
        let rv;
        debug.log ( () => `identifier ${n.name}` );
        let val = n.name;

        let source = this.findIdentifierInScope(val);
        if (source) {
            debug.log ( () => `found identifier in scope, at ${source}` );
            rv = this.createLoad(source, `load_${val}`);
            return rv;
        }

        // special handling of the arguments object here, so we
        // only initialize/create it if the function is
        // actually going to use it.
        if (val === "arguments") {
            let arguments_alloca = this.createAlloca(this.currentFunction, types.EjsValue, "local_arguments_object");
            let saved_insert_point = ir.getInsertBlock();
            ir.setInsertPoint(this.currentFunction.entry_bb);

            let load_argc = this.createLoad(this.currentFunction.topScope.get("%argc"), "argc_load");
            let load_args = this.createLoad(this.currentFunction.topScope.get("%args"), "args_load");

            let args_new = this.ejs_runtime.arguments_new;
            let arguments_object = this.createCall(args_new, [load_argc, load_args], "argstmp", !args_new.doesNotThrow);
            ir.createStore(arguments_object, arguments_alloca);
            this.currentFunction.topScope.set("arguments", arguments_alloca);

            ir.setInsertPoint(saved_insert_point);
            return this.createLoad(arguments_alloca, "load_arguments");
        }

        rv = null;
        debug.log ( () => `calling getFunction for ${val}` );
        rv = this.module.getFunction(val);

        if (!rv) {
            debug.log ( () => `Symbol '${val}' not found in current scope` );
            rv = this.loadGlobal(n);
        }

        debug.log ( () => `returning ${rv}` );
        return rv;
    }

    visitObjectExpression (n) {
        let obj_proto = ir.createLoad(this.ejs_globals.Object_prototype, "load_objproto");
        let object_create = this.ejs_runtime.object_create;
        let obj = this.createCall(object_create, [obj_proto], "objtmp", !object_create.doesNotThrow);

        let accessor_map = new Map();

        // gather all properties so we can emit get+set as a single call to define_accessor_prop.
        for (let property of n.properties) {
            if (property.kind === "get" || property.kind === "set") {
                if (!accessor_map.has(property.key)) accessor_map.set(property.key, new Map);
                if (accessor_map.get(property.key).has(property.kind))
                    throw new SyntaxError(`a '${property.kind}' method for '${escodegenerate(property.key)}' has already been defined.`);
                if (accessor_map.get(property.key).has("init"))
                    throw new SyntaxError(`${property.key.loc.start.line}: property name ${escodegenerate(property.key)} appears once in object literal.`);
            }
            else if (property.kind === "init") {
                if (accessor_map.get(property.key))
                    throw new SyntaxError(`${property.key.loc.start.line}: property name ${escodegenerate(property.key)} appears once in object literal.`);
                accessor_map.set(property.key, new Map);
            }
            else {
                throw new Error(`unrecognized property kind '${property.kind}'`);
            }
            
            if (property.computed) {
                accessor_map.get(property.key).set("computed", true);
            }
            accessor_map.get(property.key).set(property.kind, property);
        }

        accessor_map.forEach ((prop_map, propkey) => {
            // XXX we need something like this line below to handle computed properties, but those are broken at the moment
            //key = if property.key.type is Identifier then this.getAtom property.key.name else this.visit property.key

            if (prop_map.has("computed"))
                propkey = this.visit(propkey);
            else if (propkey.type == b.Literal)
                propkey = this.getAtom(String(propkey.value));
            else if (propkey.type === b.Identifier)
                propkey = this.getAtom(propkey.name);

            if (prop_map.has("init")) {
                let val = this.visit(prop_map.get("init").value);
                this.createCall(this.ejs_runtime.object_define_value_prop, [obj, propkey, val, consts.int32(0x77)], `define_value_prop_${propkey}`);
            }
            else {
                let getter = prop_map.get("get");
                let setter = prop_map.get("set");

                let get_method = getter ? this.visit(getter.value) : this.loadUndefinedEjsValue();
                let set_method = setter ? this.visit(setter.value) : this.loadUndefinedEjsValue();

                this.createCall(this.ejs_runtime.object_define_accessor_prop, [obj, propkey, get_method, set_method, consts.int32(0x19)], `define_accessor_prop_${propkey}`);
            }
        });
        
        return obj;
    }

    visitArrayExpression (n) {
        let force_fill = false;
        // if there are holes, we need to fill the array at allocation time.
        // FIXME(toshok) we could just as easily have the compiler emit code to initialize the holes as well, right?
        for (let el of n.elements) {
            if (el == null) {
                force_fill = true;
                break;
            }
        }

        let obj = this.createCall(this.ejs_runtime.array_new, [consts.int64(n.elements.length), consts.bool(force_fill)], "arrtmp", !this.ejs_runtime.array_new.doesNotThrow);
        let i = 0;
        for (let el of n.elements) {
            // don't create property stores for array holes
            if (el == null) continue;

            let val = this.visit(el);
            let index = { type: b.Literal, value: i };
            this.createPropertyStore(obj, index, val, true);
            i = i + 1;
        }
        return obj;
    }
    
    visitExpressionStatement (n) {
        n.expression.result_not_used = true;
        return this.visit(n.expression);
    }

    generateUCS2 (id, jsstr) {
        let ucsArrayType = llvm.ArrayType.get(types.JSChar, jsstr.length+1);
        let array_data = [];
        for (let i = 0, e = jsstr.length; i < e; i ++)
            array_data.push(consts.jschar(jsstr.charCodeAt(i)));
        array_data.push(consts.jschar(0));
        let array = llvm.ConstantArray.get(ucsArrayType, array_data);
        let arrayglobal = new llvm.GlobalVariable(this.module, ucsArrayType, `ucs2-${id}`, array, false);
        arrayglobal.setAlignment(8);
        return arrayglobal;
    }

    generateEJSPrimString (id, len) {
        let strglobal = new llvm.GlobalVariable(this.module, types.EjsPrimString, `primstring-${id}`, llvm.Constant.getAggregateZero(types.EjsPrimString), false);
        strglobal.setAlignment(8);
        return strglobal;
    }

    generateEJSValueForString (id) {
        let name = `ejsval-${id}`;
        let strglobal = new llvm.GlobalVariable(this.module, types.EjsValue, name, llvm.Constant.getAggregateZero(types.EjsValue), false);
        strglobal.setAlignment(8);
        let val = this.module.getOrInsertGlobal(name, types.EjsValue);
        val.setAlignment(8);
        return val;
    }
    
    addStringLiteralInitialization (name, ucs2, primstr, val, len) {
        let saved_insert_point = ir.getInsertBlock();

        ir.setInsertPointStartBB(this.literalInitializationBB);

        let saved_debug_loc;
        if (this.options.debug) {
            saved_debug_loc = ir.getCurrentDebugLocation();
            ir.setCurrentDebugLocation(llvm.DebugLoc.get(0, 0, this.literalInitializationDebugInfo));
        }

        let strname = consts.string(ir, name);

        let arg0 = strname;
        let arg1 = val;
        let arg2 = primstr;
        let arg3 = ir.createInBoundsGetElementPointer(ucs2, [consts.int32(0), consts.int32(0)], "ucs2");

        ir.createCall(this.ejs_runtime.init_string_literal, [arg0, arg1, arg2, arg3, consts.int32(len)], "");
        ir.setInsertPoint(saved_insert_point);
        if (this.options.debug)
            ir.setCurrentDebugLocation(saved_debug_loc);
    }

    getAtom (str) {
        // check if it's an atom (a runtime library constant) first of all
        if (hasOwn.call(this.ejs_atoms, str))
            return this.createLoad(this.ejs_atoms[str], `${str}_atom_load`);

        // if it's not, we create a constant and embed it in this module
        if (!this.module_atoms.has(str)) {
            let literalId = this.idgen();
            let ucs2_data = this.generateUCS2(literalId, str);
            let primstring = this.generateEJSPrimString(literalId, str.length);
            let ejsval = this.generateEJSValueForString(str);
            this.module_atoms.set(str, ejsval);
            this.addStringLiteralInitialization(str, ucs2_data, primstring, ejsval, str.length);
        }

        return this.createLoad(this.module_atoms.get(str), "literal_load");
    }
    
    visitLiteral (n) {
        // null literals, load _ejs_null
        if (n.value === null) {
            debug.log("literal: null");
            return this.loadNullEjsValue();
        }

        // undefined literals, load _ejs_undefined
        if (n.value === undefined) {
            debug.log("literal: undefined");
            return this.loadUndefinedEjsValue();
        }

        // string literals
        if (typeof n.raw === "string" && (n.raw[0] === '"' || n.raw[0] === "'")) {
            debug.log ( () => `literal string: ${n.value}` );

            var strload = this.getAtom(n.value);
            
            strload.literal = n;
            debug.log ( () => `strload = ${strload}` );
            return strload;
        }

        // regular expression literals
        if (typeof n.raw === "string" && n.raw[0] === '/') {
            debug.log ( () => `literal regexp: ${n.raw}` );

            let source = consts.string(ir, n.value.source);
            let flags = consts.string(ir, `${n.value.global ? 'g' : ''}${n.value.multiline ? 'm' : ''}${n.value.ignoreCase ? 'i' : ''}`);
            
            let regexp_new_utf8 = this.ejs_runtime.regexp_new_utf8;
            var regexpcall = this.createCall(regexp_new_utf8, [source, flags], "regexptmp", !regexp_new_utf8.doesNotThrow);
            debug.log ( () => `regexpcall = ${regexpcall}` );
            return regexpcall;
        }

        // number literals
        if (typeof n.value === "number") {
            debug.log ( () => `literal number: ${n.value}` );
            return this.loadDoubleEjsValue(n.value);
        }

        // boolean literals
        if (typeof n.value === "boolean") {
            debug.log ( () => `literal boolean: ${n.value}` );
            return this.loadBoolEjsValue(n.value);
        }

        throw `Internal error: unrecognized literal of type ${typeof n.value}`;
    }

    createCall (callee, argv, callname, canThrow=true) {
        // if we're inside a try block we have to use createInvoke, and pass two basic blocks:
        //   the normal block, which is basically this IR instruction's continuation
        //   the unwind block, where we land if the call throws an exception.
        //
        // Although for builtins we know won't throw, we can still use createCall.
        let calltmp;
        if (TryExitableScope.unwindStack.depth === 0 || callee.doesNotThrow || !canThrow) {
            //ir.createCall this.ejs_runtime.log, [consts.string(ir, `calling ${callee.name}`)], ""
            calltmp = this.abi.createCall(this.currentFunction, callee, argv, callname);
        }
        else {
            let normal_block  = new llvm.BasicBlock ("normal", this.currentFunction);
            //ir.createCall this.ejs_runtime.log, [consts.string(ir, `invoking ${callee.name}`)], ""
            calltmp = this.abi.createInvoke(this.currentFunction, callee, argv, normal_block, TryExitableScope.unwindStack.top.getLandingPadBlock(), callname);
            // after we've made our call we need to change the insertion point to our continuation
            ir.setInsertPoint(normal_block);
        }
        return calltmp;
    }
    
    visitThrow (n) {
        let arg = this.visit(n.argument);
        this.createCall(this.ejs_runtime.throw, [arg], "", true);
        return ir.createUnreachable();
    }

    visitTry (n) {
        let insertBlock = ir.getInsertBlock();
        let insertFunc = insertBlock.parent;

        let finally_block = null;
        let catch_block = null;

        // the alloca that stores the reason we ended up in the finally block
        if (!this.currentFunction.cleanup_reason)
            this.currentFunction.cleanup_reason = this.createAlloca(this.currentFunction, types.Int32, "cleanup_reason");

        // if we have a finally clause, create finally_block
        if (n.finalizer) {
            finally_block = new llvm.BasicBlock ("finally_bb", insertFunc);
            this.finallyStack.unshift (finally_block);
        }

        // the merge bb where everything branches to after falling off the end of a catch/finally block
        let merge_block = new llvm.BasicBlock ("try_merge", insertFunc);

        let branch_target = finally_block ? finally_block : merge_block;

        let scope = new TryExitableScope(this.currentFunction.cleanup_reason, branch_target, (() => new llvm.BasicBlock("exception", insertFunc)), finally_block != null);
        this.doInsideExitableScope (scope, () => {
            scope.enterTry();
            this.visit(n.block);

            if (n.finalizer)
                this.finallyStack.shift();
            
            // at the end of the try block branch to our branch_target (either the finally block or the merge block after the try{}) with REASON_FALLOFF
            scope.exitAft(false);
            scope.leaveTry();
        });

        if (scope.landing_pad_block && n.handlers.length > 0)
            catch_block = new llvm.BasicBlock ("catch_bb", insertFunc);


        if (scope.landing_pad_block) {
            // the scope's landingpad block is created if needed by this.createCall (using that function we pass in as the last argument to TryExitableScope's ctor.)
            // if a try block includes no calls, there's no need for an landing pad block as nothing can throw, and we don't bother generating any code for the
            // catch clause.
            this.doInsideBBlock (scope.landing_pad_block, () => {

                let landing_pad_type = llvm.StructType.create("", [types.Int8Pointer, types.Int32]);
                // XXX is it an error to have multiple catch handlers, as JS doesn't allow you to filter by type?
                let clause_count = n.handlers.length > 0 ? 1 : 0;
                
                let casted_personality = ir.createPointerCast(this.ejs_runtime.personality, types.Int8Pointer, "personality");
                let caught_result = ir.createLandingPad(landing_pad_type, casted_personality, clause_count, "caught_result");
                caught_result.addClause(ir.createPointerCast(this.ejs_runtime.exception_typeinfo, types.Int8Pointer, ""));
                caught_result.setCleanup(true);

                let exception = ir.createExtractValue(caught_result, 0, "exception");
                
                if (catch_block)
                    ir.createBr(catch_block);
                else if (finally_block)
                    ir.createBr(finally_block);
                else
                    throw "this shouldn't happen.  a try{} without either a catch{} or finally{}";

                // if we have a catch clause, create catch_bb
                if (n.handlers.length > 0) {
                    this.doInsideBBlock (catch_block, () => {
                        // call _ejs_begin_catch to return the actual exception
                        let catchval = this.beginCatch(exception);
                        
                        // create a new scope which maps the catch parameter name (the "e" in "try { } catch (e) { }") to catchval
                        let catch_scope = new Map;
                        if (n.handlers[0].param && n.handlers[0].param.name) {
                            let catch_name = n.handlers[0].param.name;
                            let alloca = this.createAlloca(this.currentFunction, types.EjsValue, `local_catch_${catch_name}`);
                            catch_scope.set(catch_name, alloca);
                            ir.createStore(catchval, alloca);
                        }

                        if (n.finalizer)
                            this.finallyStack.unshift(finally_block);

                        this.doInsideExitableScope(scope, () => {
                            this.visitWithScope(catch_scope, [n.handlers[0]]);
                        });

                        // unsure about this one - we should likely call end_catch if another exception is thrown from the catch block?
                        this.endCatch();

                        if (n.finalizer)
                            this.finallyStack.shift();
                
                        // at the end of the catch block branch to our branch_target (either the finally block or the merge block after the try{}) with REASON_FALLOFF
                        scope.exitAft(false);
                    });
                }
            });
        }

        // Finally Block
        if (n.finalizer) {
            this.doInsideBBlock (finally_block, () => {
                this.visit(n.finalizer);

                let cleanup_reason = this.createLoad(this.currentFunction.cleanup_reason, "cleanup_reason_load");

                let return_tramp = null;
                if (this.currentFunction.returnValueAlloca) {
                    return_tramp = new llvm.BasicBlock ("return_tramp", insertFunc);
                    this.doInsideBBlock (return_tramp, () => {
                        if (this.finallyStack.length > 0) {
                            ir.createStore(consts.int32(ExitableScope.REASON_RETURN), this.currentFunction.cleanup_reason);
                            ir.createBr(this.finallyStack[0]);
                        }
                        else {
                            this.createRet(this.createLoad(this.currentFunction.returnValueAlloca, "rv"));
                        }
                    });
                }
                
                let switch_stmt = ir.createSwitch(cleanup_reason, merge_block, scope.destinations.length + 1);
                if (this.currentFunction.returnValueAlloca)
                    switch_stmt.addCase(consts.int32(ExitableScope.REASON_RETURN), return_tramp);

                let falloff_tramp = new llvm.BasicBlock("falloff_tramp", insertFunc);
                this.doInsideBBlock (falloff_tramp, () => {
                    ir.createBr(merge_block);
                });
                switch_stmt.addCase(consts.int32(TryExitableScope.REASON_FALLOFF_TRY), falloff_tramp);

                for (let s = 0, e = scope.destinations.length; s < e; s ++) {
                    let dest_tramp = new llvm.BasicBlock("dest_tramp", insertFunc);
                    var dest = scope.destinations[s];
                    this.doInsideBBlock (dest_tramp, () => {
                        if (dest.reason == TryExitableScope.REASON_BREAK)
                            dest.scope.exitAft(true);
                        else if (dest.reason == TryExitableScope.REASON_CONTINUE)
                            dest.scope.exitFore();
                    });
                    switch_stmt.addCase(dest.id, dest_tramp);
                }
            });
        }
        
        ir.setInsertPoint(merge_block);
    }

    handleTemplateDefaultHandlerCall (exp, opencode) {
        // we should probably only inline the construction of the string if substitutions.length < $some-number
        let cooked_strings = exp.arguments[0].elements;
        let substitutions = exp.arguments[1].elements;
        
        let cooked_i = 0;
        let sub_i = 0;
        let strval = null;

        let concat_string = (s) => {
            if (!strval)
                strval = s;
            else
                strval = this.createCall(this.ejs_runtime.string_concat, [strval, s], "strconcat");
        };
        
        while (cooked_i < cooked_strings.length) {
            let c = cooked_strings[cooked_i];
            cooked_i += 1;
            if (c.length !== 0)
                concat_string(this.getAtom(c.value));
            if (sub_i < substitutions.length) {
                let sub = this.visit(substitutions[sub_i]);
                concat_string(this.createCall(this.ejs_runtime.ToString, [sub], "subToString"));
                sub_i += 1;
            }
        }

        return strval;
    }

    handleTemplateCallsite (exp, opencode) {
        // we expect to be called with context something of the form:
        //
        //   function generate_callsiteId0 () {
        //       %templateCallsite(%callsiteId_0,
        //           [], // raw
        //           []  // cooked
        //       });
        //   }
        // 
        // and we need to generate something along the lines of:
        //
        //   global const %callsiteId_0 = null; // an llvm IR construct
        //
        //   function generate_callsiteId0 () {
        //       if (!%callsiteId_0) {
        //           _ejs_gc_add_root(&%callsiteId_0);
        //           %callsiteId_0 = []; // cooked
        //           %callsiteId_0.raw = [];
        //           %callsiteId_0.freeze();
        //       }
        //       return callsiteId_0;
        //   }
        //
        // our containing function already exists, so we just
        // need to replace the intrinsic with the new contents.
        //
        // XXX there's no reason to dynamically create the
        // callsite, other than it being easier for now.  The
        // callsite id's structure is known at compile time so
        // everything could be allocated from the data segment
        // and just used from there (much the same way we do
        // with string literals.)

        let callsite_id = exp.arguments[0].value;
        let callsite_raw_literal = exp.arguments[1];
        let callsite_cooked_literal = exp.arguments[2];

        let insertBlock = ir.getInsertBlock();
        let insertFunc = insertBlock.parent;

        let then_bb   = new llvm.BasicBlock ("then",  insertFunc);
        let merge_bb  = new llvm.BasicBlock ("merge", insertFunc);

        let callsite_alloca = this.createAlloca(this.currentFunction, types.EjsValue, `local_${callsite_id}`);

        let callsite_global = new llvm.GlobalVariable(this.module, types.EjsValue, callsite_id, llvm.Constant.getAggregateZero(types.EjsValue), false);
        let global_callsite_load = this.createLoad(callsite_global, "load_global_callsite");
        ir.createStore(global_callsite_load, callsite_alloca);

        let callsite_load = ir.createLoad(callsite_alloca, "load_local_callsite");

        let isnull = this.isNumber(callsite_load);
        ir.createCondBr(isnull, then_bb, merge_bb);

        this.doInsideBBlock (then_bb, () => {
            this.createCall(this.ejs_runtime.gc_add_root, [callsite_global], "");
            // XXX missing: register callsite_obj gc root
            let callsite_cooked = this.visit(callsite_cooked_literal);
            let callsite_raw = this.visit(callsite_raw_literal);

            let frozen_raw = this.createCall(this.ejs_runtime.object_freeze, [callsite_raw], "frozen_raw");

            this.createCall(this.ejs_runtime.object_setprop, [callsite_cooked, this.visit(b.literal("raw")), frozen_raw], "propstore_raw");

            let frozen_cooked = this.createCall(this.ejs_runtime.object_freeze, [callsite_cooked], "frozen_cooked");
            ir.createStore(frozen_cooked, callsite_global);
            ir.createStore(frozen_cooked, callsite_alloca);
            ir.createBr(merge_bb);
        });
        
        ir.setInsertPoint(merge_bb);
        return this.createRet(ir.createLoad(callsite_alloca, "load_local_callsite"));
    }

    handleModuleGet (exp, opencode) {
        let moduleString = this.visit(exp.arguments[0].value);
        return this.createCall(this.ejs_runtime.module_get, [moduleString], "moduletmp");
    }

    handleModuleSlotRef (exp, opencode) {
        let moduleString = exp.arguments[0].value;
        let exportId = exp.arguments[1].value;
        let module_global;

        if (moduleString.endsWith(".js"))
            moduleString = moduleString.substring(0, moduleString.length-3);
        if (moduleString === this.this_module_info.path) {
            module_global = this.this_module_global;
        }
        else {
            module_global = this.import_module_globals.get(moduleString);
        }
        module_global = ir.createPointerCast(module_global, types.EjsModule.pointerTo(), "");

        let slotnum = this.allModules.get(moduleString).exports.get(exportId).slot_num;

        if (opencode && this.options.target_pointer_size === 64) {
            return ir.createInBoundsGetElementPointer(module_global, [consts.int64(0), consts.int32(3), consts.int64(slotnum)], "slot_ref");
        }
        else {
            return this.createCall(this.ejs_runtime.module_get_slot_ref, [module_global, consts.int32(slotnum)], "module_slot");
        }
    }

    handleModuleGetSlot (exp, opencode) {
        let slot_ref = this.handleModuleSlotRef(exp, opencode);
        return ir.createLoad(slot_ref, "module_slot_load");
    }

    handleModuleSetSlot (exp, opencode) {
        let arg = exp.arguments[2];

        let slot_ref = this.handleModuleSlotRef(exp, opencode);
        this.storeToDest(slot_ref, arg);

        return ir.createLoad(slot_ref, "load_slot"); // do we need this?  we don't need to keep assignment expression semantics for this
    }

    handleModuleGetExotic (exp, opencode) {
/*
        let moduleString = exp.arguments[0].value;

        if (moduleString === this.this_module_info.path) {
            let module_global = this.this_module_global;
            return this.emitEjsvalFromPtr(module_global, "exotic");
        }
        else if (this.import_module_globals.has(moduleString)) {
            let module_global = this.import_module_globals.get(moduleString)
            return this.emitEjsvalFromPtr(module_global, "exotic");
        }
        else {
*/
            return this.createCall(this.ejs_runtime.module_get, [this.visit(exp.arguments[0])], "get_module_exotic");
/*
        }
*/
    }
    
    handleGetNewTarget (exp, opencode) {
        return this.createLoad(this.findIdentifierInScope("%newTarget"), "new_target_load");
    }

    handleGetArgumentsObject (exp, opencode) {
        let arguments_alloca = this.createAlloca(this.currentFunction, types.EjsValue, "local_arguments_object");
        let saved_insert_point = ir.getInsertBlock();
        ir.setInsertPoint(this.currentFunction.entry_bb);

        let load_argc = this.createLoad(this.currentFunction.topScope.get("%argc"), "argc_load");
        let load_args = this.createLoad(this.currentFunction.topScope.get("%args"), "args_load");

        let args_new = this.ejs_runtime.arguments_new;
        let arguments_object = this.createCall(args_new, [load_argc, load_args], "argstmp", !args_new.doesNotThrow);
        ir.createStore(arguments_object, arguments_alloca);
        this.currentFunction.topScope.set("arguments", arguments_alloca);

        ir.setInsertPoint(saved_insert_point);
        return this.createLoad(arguments_alloca, "load_arguments");
    }

    handleGetLocal  (exp, opencode) { return this.createLoad(this.findIdentifierInScope(exp.arguments[0].name), `load_${exp.arguments[0].name}`); }
    handleGetGlobal (exp, opencode) { return this.loadGlobal(exp.arguments[0]); }

    handleSetLocal (exp, opencode) {
        let dest = this.findIdentifierInScope(exp.arguments[0].name);
        if (!dest)
            throw new Error(`identifier not found: ${exp.arguments[0].name}`);
        let arg = exp.arguments[1];
        this.storeToDest(dest, arg);
        return ir.createLoad(dest, "load_val");
    }
    
    handleSetGlobal (exp, opencode) {
        let gname = exp.arguments[0].name;

        if (this.options.frozen_global)
            throw new SyntaxError(`cannot set global property '${exp.arguments[0].name}' when using --frozen-global`);
        
        let gatom = this.getAtom(gname);
        let value = this.visit(exp.arguments[1]);

        return this.createCall(this.ejs_runtime.global_setprop, [gatom, value], `globalpropstore_${gname}`);
    }

    // this method assumes it's called in an opencoded context
    emitEjsvalTo (val, type, prefix) {
        if (this.options.target_pointer_size === 64) {
            let payload = this.createEjsvalAnd(val, consts.int64_lowhi(0x7fff, 0xffffffff), `${prefix}_payload`);
            return ir.createIntToPtr(payload, type, `${prefix}_load`);
        }
        else {
            throw new Error("emitEjsvalTo not implemented for this case");
        }
    }

    emitEjsvalFromPtr (ptr, prefix)  {
        if (this.options.target_pointer_size === 64) {
            let fromptr_alloca = this.createAlloca(this.currentFunction, types.EjsValue, `${prefix}_ejsval`);
            let intval = ir.createPtrToInt(ptr, types.Int64, `${prefix}_intval`);
            let payload = ir.createOr(intval, consts.int64_lowhi(0xFFFC0000, 0x00000000), `${prefix}_payload`);
            let alloca_as_int64 = ir.createBitCast(fromptr_alloca, types.Int64.pointerTo(), `${prefix}_alloca_asptr`);
            ir.createStore(payload, alloca_as_int64, `${prefix}_store`);
            ir.createLoad(fromptr_alloca, `${prefix}_load`);
        }
        else {
            throw new Error("emitEjsvalTo not implemented for this case");
        }
    }

    emitEjsvalToObjectPtr (val) {
        return this.emitEjsvalTo(val, types.EjsObject.pointerTo(), "to_objectptr");
    }

    emitEjsvalToClosureEnvPtr (val) {
        return this.emitEjsvalTo(val, types.EjsClosureEnv.pointerTo(), "to_ptr");
    }
    
    // this method assumes it's called in an opencoded context
    emitLoadSpecops (obj) {
        if (this.options.target_pointer_size === 64) {
            // %1 = getelementptr inbounds %struct._EJSObject* %obj, i64 0, i32 1
            // %specops_load = load %struct.EJSSpecOps** %1, align 8, !tbaa !0
            let specops_slot = ir.createInBoundsGetElementPointer(obj, [consts.int64(0), consts.int32(1)], "specops_slot");
            return ir.createLoad(specops_slot, "specops_load");
        }
        else {
            throw new Error("emitLoadSpecops not implemented for this case");
        }
    }

    emitThrowNativeError (errorCode, errorMessage) {
        this.createCall(this.ejs_runtime.throw_nativeerror_utf8, [consts.int32(errorCode), consts.string(ir, errorMessage)], "", true);
        return ir.createUnreachable();
    }

    // this method assumes it's called in an opencoded context
    emitLoadEjsFunctionClosureFunc (closure) {
        if (this.options.target_pointer_size === 64) {
            let func_slot_gep = ir.createInBoundsGetElementPointer(closure, [consts.int64(1)], "func_slot_gep");
            let func_slot = ir.createBitCast(func_slot_gep, this.abi.createFunctionType(types.EjsValue, [types.EjsValue, types.EjsValue, types.Int32, types.EjsValue.pointerTo()]).pointerTo().pointerTo(), "func_slot");
            return ir.createLoad(func_slot, "func_load");
        }
        else {
            throw new Error("emitLoadEjsFunctionClosureFunc not implemented for this case");
        }
    }
    
    // this method assumes it's called in an opencoded context
    emitLoadEjsFunctionClosureEnv (closure) {
        if (this.options.target_pointer_size === 64) {
            let env_slot_gep = ir.createInBoundsGetElementPointer(closure, [consts.int64(1), consts.int32(1)], "env_slot_gep");
            let env_slot = ir.createBitCast(env_slot_gep, types.EjsValue.pointerTo(), "env_slot");
            return ir.createLoad(env_slot, "env_load");
        }
        else {
            throw new Error("emitLoadEjsFunctionClosureEnv not implemented for this case");
        }
    }
    
    handleInvokeClosure (exp, opencode) {
        let insertBlock = ir.getInsertBlock();
        let insertFunc = insertBlock.parent;

        if (!this.currentFunction.scratch_area) {
            throw new Error(`Internal error: function has no scratch space and makes a [[Call]] call with ${exp.arguments.length} arguments`);
        }

        let argv = this.visitArgsForCall(this.ejs_runtime.invoke_closure, true, exp.arguments);

        if (opencode && this.options.target_pointer_size === 64) {
            //
            // generate basically the following code:
            //
            // f = argv[0]
            // if (EJSVAL_IS_FUNCTION(F)
            //   f->func(f->env, argv[1], argv[2], argv[3])
            // else
            //   _ejs_invoke_closure(...argv)
            // 
            let candidate_is_object_bb = new llvm.BasicBlock ("candidate_is_object_bb", insertFunc);
            var direct_invoke_bb = new llvm.BasicBlock ("direct_invoke_bb", insertFunc);
            var runtime_invoke_bb = new llvm.BasicBlock ("runtime_invoke_bb", insertFunc);
            var invoke_merge_bb = new llvm.BasicBlock ("invoke_merge_bb", insertFunc);
            
            let cmp = this.isObject(argv[0]);
            ir.createCondBr(cmp, candidate_is_object_bb, runtime_invoke_bb);

            var call_result_alloca = this.createAlloca(this.currentFunction, types.EjsValue, "call_result");
            
            this.doInsideBBlock (candidate_is_object_bb, () => {
                let closure = this.emitEjsvalToObjectPtr(argv[0]);
                let cmp = this.isObjectFunction(closure);

                ir.createCondBr(cmp, direct_invoke_bb, runtime_invoke_bb);

                // in the successful case we modify our argv with the responses and directly invoke the closure func
                this.doInsideBBlock (direct_invoke_bb, () => {
                    let func_load = this.emitLoadEjsFunctionClosureFunc(closure);
                    let env_load = this.emitLoadEjsFunctionClosureEnv(closure);
                    let direct_call_result = this.createCall(func_load, [env_load, argv[1], argv[2], argv[3], argv[4], argv[5]], "callresult");
                    ir.createStore(direct_call_result, call_result_alloca);
                    ir.createBr(invoke_merge_bb);
                });

                this.doInsideBBlock (runtime_invoke_bb, () => {
                    let runtime_call_result = this.createCall(this.ejs_runtime.invoke_closure, argv, "callresult", true);
                    ir.createStore(runtime_call_result, call_result_alloca);
                    ir.createBr(invoke_merge_bb);
                });
            });

            ir.setInsertPoint(invoke_merge_bb);

            return ir.createLoad(call_result_alloca, "call_result_load");
        }
        else {
            return this.createCall(this.ejs_runtime.invoke_closure, argv, "call", true);
        }
    }

    handleConstructClosure (exp, opencode) {
        let this_alloca = this.createAlloca(this.currentFunction, types.EjsValue, "this_alloca");
        this.storeUndefined(this_alloca, "store_undefined_this");

        if (!this.currentFunction.scratch_area) {
            throw new Error(`Internal error: function has no scratch space and makes a [[Construct]] call with ${exp.arguments.length} arguments`);
        }

        let argv = this.visitArgsForConstruct(this.ejs_runtime.construct_closure, exp.arguments, this_alloca);

        return this.createCall(this.ejs_runtime.construct_closure, argv, "construct", true);
    }

    handleConstructSuper (exp, opencode) {
        let this_ptr = this.createLoad(this.findIdentifierInScope("%this"), "this_ptr");
        let newTarget = this.createLoad(this.findIdentifierInScope("%newTarget"), "load_newTarget");

        let argv = this.visitArgsForConstruct(this.ejs_runtime.construct_closure, exp.arguments, this_ptr, newTarget);
        
        return this.createCall(this.ejs_runtime.construct_closure, argv, "construct_super", true);
    }

    handleConstructSuperApply (exp, opencode) {
        let this_ptr = this.createLoad(this.findIdentifierInScope("%this"), "this_ptr");
        let newTarget = this.createLoad(this.findIdentifierInScope("%newTarget"), "load_newTarget");

        let argv = this.visitArgsForConstruct(this.ejs_runtime.construct_closure_apply, exp.arguments, this_ptr, newTarget);
        
        return this.createCall(this.ejs_runtime.construct_closure_apply, argv, "construct_super_apply", true);
    }

    handleSetConstructorKindDerived (exp, opencode) {
        let ctor = this.visit(exp.arguments[0]);
        return this.createCall(this.ejs_runtime.set_constructor_kind_derived, [ctor], "");
    }

    handleSetConstructorKindBase (exp, opencode) {
        let ctor = this.visit(exp.arguments[0]);
        return this.createCall(this.ejs_runtime.set_constructor_kind_base, [ctor], "");
    }

    handleMakeGenerator (exp, opencode) {
        let argv = this.visitArgsForCall(this.ejs_runtime.make_generator, false, exp.arguments);
        return this.createCall(this.ejs_runtime.make_generator, argv, "generator");
    }
                
    handleGeneratorYield (exp, opencode) {
        let argv = this.visitArgsForCall(this.ejs_runtime.generator_yield, false, exp.arguments);
        return this.createCall(this.ejs_runtime.generator_yield, argv, "yield");
    }
    
    handleMakeClosure (exp, opencode) {
        let argv = this.visitArgsForCall(this.ejs_runtime.make_closure, false, exp.arguments);
        return this.createCall(this.ejs_runtime.make_closure, argv, "closure_tmp");
    }

    handleMakeClosureNoEnv (exp, opencode) {
        let argv = this.visitArgsForCall(this.ejs_runtime.make_closure_noenv, false, exp.arguments);
        return this.createCall(this.ejs_runtime.make_closure_noenv, argv, "closure_tmp");
    }

    handleMakeAnonClosure (exp, opencode) {
        let argv = this.visitArgsForCall(this.ejs_runtime.make_anon_closure, false, exp.arguments);
        return this.createCall(this.ejs_runtime.make_anon_closure, argv, "closure_tmp");
    }
    
    handleCreateArgScratchArea (exp, opencode) {
        let argsArrayType = llvm.ArrayType.get(types.EjsValue, exp.arguments[0].value);
        this.currentFunction.scratch_length = exp.arguments[0].value;
        this.currentFunction.scratch_area = this.createAlloca(this.currentFunction, argsArrayType, "args_scratch_area");
        this.currentFunction.scratch_area.setAlignment(8);
        return this.currentFunction.scratch_area;
    }

    handleMakeClosureEnv (exp, opencode) {
        let size = exp.arguments[0].value;
        return this.createCall(this.ejs_runtime.make_closure_env, [consts.int32(size)], "env_tmp");
    }

    handleGetSlot (exp, opencode) {
        //
        //  %ref = handleSlotRef
        //  %ret = load %EjsValueType* %ref, align 8
        //
        let slot_ref = this.handleSlotRef(exp, opencode);
        return ir.createLoad(slot_ref, "slot_ref_load");
    }

    handleSetSlot (exp, opencode) {
        let new_slot_val;

        if (exp.arguments.length === 4)
            new_slot_val = exp.arguments[3];
        else
            new_slot_val = exp.arguments[2];

        let slotref = this.handleSlotRef(exp, opencode);
        
        this.storeToDest(slotref, new_slot_val);
        
        return ir.createLoad(slotref, "load_slot");
    }

    handleSlotRef (exp, opencode) {
        let env = this.visitOrNull(exp.arguments[0]);
        let slotnum = exp.arguments[1].value;

        if (opencode && this.options.target_pointer_size === 64) {
            let envp = this.emitEjsvalToClosureEnvPtr(env);
            return ir.createInBoundsGetElementPointer(envp, [consts.int64(0), consts.int32(2), consts.int64(slotnum)], "slot_ref");
        }
        else {
            return this.createCall(this.ejs_runtime.get_env_slot_ref, [env, consts.int32(slotnum)], "slot_ref_tmp", false);
        }
    }

    createEjsBoolSelect (val, falseval = false) {
        let rv = ir.createSelect(val, this.loadBoolEjsValue(!falseval), this.loadBoolEjsValue(falseval), "sel");
        rv._ejs_returns_ejsval_bool = true;
        return rv;
    }

    getEjsvalBits (arg) {
        let bits_alloca;

        if (this.currentFunction.bits_alloca)
            bits_alloca = this.currentFunction.bits_alloca;
        else
            bits_alloca = this.createAlloca(this.currentFunction, types.EjsValue, "bits_alloca");

        ir.createStore(arg, bits_alloca);
        let bits_ptr = ir.createBitCast(bits_alloca, types.Int64.pointerTo(), "bits_ptr");
        if (!this.currentFunction.bits_alloca)
            this.currentFunction.bits_alloca = bits_alloca;
        return ir.createLoad(bits_ptr, "bits_load");
    }
    
    createEjsvalICmpUGt (arg, i64_const, name) { return ir.createICmpUGt(this.getEjsvalBits(arg), i64_const, name); }
    createEjsvalICmpULt (arg, i64_const, name) { return ir.createICmpULt(this.getEjsvalBits(arg), i64_const, name); }
    createEjsvalICmpEq  (arg, i64_const, name) { return ir.createICmpEq (this.getEjsvalBits(arg), i64_const, name); }
    createEjsvalAnd     (arg, i64_const, name) { return ir.createAnd    (this.getEjsvalBits(arg), i64_const, name); }

    isObject (val) {
        if (this.options.target_pointer_size === 64) {
            return this.createEjsvalICmpUGt(val, consts.int64_lowhi(0xfffbffff, 0xffffffff), "cmpresult");
        }
        else {
            let mask = this.createEjsvalAnd(val, consts.int64_lowhi(0xffffffff, 0x00000000), "mask.i");
            return ir.createICmpEq(mask, consts.int64_lowhi(0xffffff88, 0x00000000), "cmpresult");
        }
    }

    isObjectFunction (obj) {
        return ir.createICmpEq(this.emitLoadSpecops(obj), this.ejs_runtime.function_specops, "function_specops_cmp");
    }

    isObjectSymbol (obj) {
        return ir.createICmpEq(this.emitLoadSpecops(obj), this.ejs_runtime.symbol_specops, "symbol_specops_cmp");
    }

    isString (val) {
        if (this.options.target_pointer_size === 64) {
            let mask = this.createEjsvalAnd(val, consts.int64_lowhi(0xffff8000, 0x00000000), "mask.i");
            return ir.createICmpEq(mask, consts.int64_lowhi(0xfffa8000, 0x00000000), "cmpresult");
        }
        else {
            let mask = this.createEjsvalAnd(val, consts.int64_lowhi(0xffffffff, 0x00000000), "mask.i");
            return ir.createICmpEq(mask, consts.int64_lowhi(0xffffff85, 0x00000000), "cmpresult");
        }
    }

    isNumber (val) {
        if (this.options.target_pointer_size === 64) {
            return this.createEjsvalICmpULt(val, consts.int64_lowhi(0xfff80001, 0x00000000), "cmpresult");
        }
        else {
            let mask = this.createEjsvalAnd(val, consts.int64_lowhi(0xffffffff, 0x00000000), "mask.i");
            return ir.createICmpULt(mask, consts.int64_lowhi(0xffffff81, 0x00000000), "cmpresult");
        }
    }
    
    isBoolean (val) {
        if (this.options.target_pointer_size === 64) {
            let mask = this.createEjsvalAnd(val, consts.int64_lowhi(0xffff8000, 0x00000000), "mask.i");
            return ir.createICmpEq(mask, consts.int64_lowhi(0xfff98000, 0x00000000), "cmpresult");
        }
        else {
            let mask = this.createEjsvalAnd(val, consts.int64_lowhi(0xffffffff, 0x00000000), "mask.i");
            return ir.createICmpEq(mask, consts.int64_lowhi(0xffffff83, 0x00000000), "cmpresult");
        }
    }

    // these two could/should be changed to check for the specific bitpattern of _ejs_true/_ejs_false
    isTrue  (val) { return ir.createICmpEq(val, consts.ejsval_true(), "cmpresult"); }
    isFalse (val) { return ir.createICmpEq(val, consts.ejsval_false(), "cmpresult"); }
    
    isUndefined (val) {
        if (this.options.target_pointer_size === 64) {
            return this.createEjsvalICmpEq(val, consts.int64_lowhi(0xfff90000, 0x00000000), "cmpresult");
        }
        else {
            let mask = this.createEjsvalAnd(val, consts.int64_lowhi(0xffffffff, 0x00000000), "mask.i");
            return ir.createICmpEq(mask, consts.int64_lowhi(0xffffff82, 0x00000000), "cmpresult");
        }
    }

    isNull (val) {
        if (this.options.target_pointer_size === 64) {
            return this.createEjsvalICmpEq(val, consts.int64_lowhi(0xfffb8000, 0x00000000), "cmpresult");
        }
        else {
            let mask = this.createEjsvalAnd(val, consts.int64_lowhi(0xffffffff, 0x00000000), "mask.i");
            return ir.createICmpEq(mask, consts.int64_lowhi(0xffffff87, 0x00000000), "cmpresult");
        }
    }

    handleTypeofIsObject (exp, opencode) {
        let arg = this.visitOrNull(exp.arguments[0]);
        return this.createEjsBoolSelect(this.isObject(arg));
    }

    handleTypeofIsFunction (exp, opencode) {
        let arg = this.visitOrNull(exp.arguments[0]);
        if (opencode && this.options.target_pointer_size === 64) {
            let insertBlock = ir.getInsertBlock();
            let insertFunc  = insertBlock.parent;
            
            var typeofIsFunction_alloca = this.createAlloca(this.currentFunction, types.EjsValue, "typeof_is_function");
            
            var failure_bb   = new llvm.BasicBlock ("typeof_function_false",     insertFunc);
            let is_object_bb = new llvm.BasicBlock ("typeof_function_is_object", insertFunc);
            var success_bb   = new llvm.BasicBlock ("typeof_function_true",      insertFunc);
            var merge_bb     = new llvm.BasicBlock ("typeof_function_merge",     insertFunc);

            let cmp = this.isObject(arg, true);
            ir.createCondBr(cmp, is_object_bb, failure_bb);

            this.doInsideBBlock (is_object_bb, () => {
                let obj = this.emitEjsvalToObjectPtr(arg);
                let cmp = this.isObjectFunction(obj);
                ir.createCondBr(cmp, success_bb, failure_bb);
            });

            this.doInsideBBlock (success_bb, () => {
                this.storeBoolean(typeofIsFunction_alloca, true, "store_typeof");
                ir.createBr(merge_bb);
            });
            
            this.doInsideBBlock (failure_bb, () => {
                this.storeBoolean(typeofIsFunction_alloca, false, "store_typeof");
                ir.createBr(merge_bb);
            });

            ir.setInsertPoint(merge_bb);
            
            let rv = ir.createLoad(typeofIsFunction_alloca, "typeof_is_function");
            rv._ejs_returns_ejsval_bool = true;
            return rv;
        }
        else {
            return this.createCall(this.ejs_runtime.typeof_is_function, [arg], "is_function", false);
        }
    }

    handleTypeofIsSymbol (exp, opencode) {
        let arg = this.visitOrNull(exp.arguments[0]);
        if (opencode && this.options.target_pointer_size === 64) {
            let insertBlock = ir.getInsertBlock();
            let insertFunc  = insertBlock.parent;
            
            var typeofIsSymbol_alloca = this.createAlloca(this.currentFunction, types.EjsValue, "typeof_is_symbol");
            
            var failure_bb   = new llvm.BasicBlock ("typeof_symbol_false",     insertFunc);
            let is_object_bb = new llvm.BasicBlock ("typeof_symbol_is_object", insertFunc);
            var success_bb   = new llvm.BasicBlock ("typeof_symbol_true",      insertFunc);
            var merge_bb     = new llvm.BasicBlock ("typeof_symbol_merge",     insertFunc);
            
            let cmp = this.isObject(arg, true);
            ir.createCondBr(cmp, is_object_bb, failure_bb);

            this.doInsideBBlock (is_object_bb, () => {
                let obj = this.emitEjsvalToObjectPtr(arg);
                let cmp = this.isObjectSymbol(obj);
                ir.createCondBr(cmp, success_bb, failure_bb);
            });

            this.doInsideBBlock (success_bb, () => {
                this.storeBoolean(typeofIsSymbol_alloca, true, "store_typeof");
                ir.createBr(merge_bb);
            });

            this.doInsideBBlock (failure_bb, () => {
                this.storeBoolean(typeofIsSymbol_alloca, false, "store_typeof");
                ir.createBr(merge_bb);
            });

            ir.setInsertPoint(merge_bb);
            
            let rv = ir.createLoad(typeofIsSymbol_alloca, "typeof_is_symbol");
            rv._ejs_returns_ejsval_bool = true;
            return rv;
        }
        else {
            return this.createCall(this.ejs_runtime.typeof_is_symbol, [arg], "is_symbol", false);
        }
    }

    handleTypeofIsString (exp, opencode) {
        let arg = this.visitOrNull(exp.arguments[0]);
        return this.createEjsBoolSelect(this.isString(arg));
    }
    
    handleTypeofIsNumber    (exp, opencode) {
        let arg = this.visitOrNull(exp.arguments[0]);
        return this.createEjsBoolSelect(this.isNumber(arg));
    }

    handleTypeofIsBoolean   (exp, opencode) {
        let arg = this.visitOrNull(exp.arguments[0]);
        return this.createEjsBoolSelect(this.isBoolean(arg));
    }

    handleIsUndefined (exp, opencode) {
        let arg = this.visitOrNull(exp.arguments[0]);
        return this.createEjsBoolSelect(this.isUndefined(arg));
    }
    
    handleIsNull (exp, opencode) {
        let arg = this.visitOrNull(exp.arguments[0]);
        return this.createEjsBoolSelect(this.isNull(arg));
    }
    
    handleIsNullOrUndefined (exp, opencode) {
        let arg = this.visitOrNull(exp.arguments[0]);
        if (opencode)
            return this.createEjsBoolSelect(ir.createOr(this.isNull(arg), this.isUndefined(arg), "or"));
        else
            return this.createCall(this.ejs_binops["=="],   [this.loadNullEjsValue(), arg], "is_null_or_undefined", false);
    }
    
    handleBuiltinUndefined (exp) { return this.loadUndefinedEjsValue(); }

    handleSetPrototypeOf  (exp) {
        let obj   = this.visitOrNull (exp.arguments[0]);
        let proto = this.visitOrNull (exp.arguments[1]);
        return this.createCall(this.ejs_runtime.object_set_prototype_of, [obj, proto], "set_prototype_of", true);
        // we should check the return value of set_prototype_of
    }

    handleObjectCreate  (exp) {
        let proto = this.visitOrNull(exp.arguments[0]);
        return this.createCall(this.ejs_runtime.object_create, [proto], "object_create", true);
        // we should check the return value of object_create
    }

    handleArrayFromRest (exp) {
        let rest_name = exp.arguments[0].value;
        let formal_params_length = exp.arguments[1].value;
        
        let has_rest_bb = new llvm.BasicBlock ("has_rest_bb", this.currentFunction);
        let no_rest_bb = new llvm.BasicBlock ("no_rest_bb", this.currentFunction);
        let rest_merge_bb = new llvm.BasicBlock ("rest_merge", this.currentFunction);
        
        let rest_alloca = this.createAlloca(this.currentFunction, types.EjsValue, "local_rest_object");

        let load_argc = this.createLoad(this.currentFunction.topScope.get("%argc"), "argc_load");

        let cmp = ir.createICmpSGt(load_argc, consts.int32(formal_params_length), "argcmpresult");
        ir.createCondBr(cmp, has_rest_bb, no_rest_bb);

        ir.setInsertPoint(has_rest_bb);
        // we have > args than are declared, shove the rest into the rest parameter
        let load_args = this.createLoad(this.currentFunction.topScope.get("%args"), "args_load");
        let gep = ir.createInBoundsGetElementPointer(load_args, [consts.int32(formal_params_length)], "rest_arg_gep");
        load_argc = ir.createNswSub(load_argc, consts.int32(formal_params_length));
        load_argc = ir.createZExt(load_argc, types.Int64);
        let rest_value = this.createCall(this.ejs_runtime.array_new_copy, [load_argc, gep], "argstmp", !this.ejs_runtime.array_new_copy.doesNotThrow);
        ir.createStore(rest_value, rest_alloca);
        ir.createBr(rest_merge_bb);

        ir.setInsertPoint(no_rest_bb);
        // we have <= args than are declared, so the rest parameter is just an empty array
        rest_value = this.createCall(this.ejs_runtime.array_new, [consts.int64(0), consts.False()], "arrtmp", !this.ejs_runtime.array_new.doesNotThrow);
        ir.createStore(rest_value, rest_alloca);
        ir.createBr(rest_merge_bb);

        ir.setInsertPoint(rest_merge_bb);
        
        this.currentFunction.topScope.set(rest_name, rest_alloca);
        this.currentFunction.restArgPresent = true;

        return ir.createLoad(rest_alloca, "load_rest");
    }

    handleArrayFromSpread (exp) {
        let arg_count = exp.arguments.length;
        let spread_alloca = this.currentFunction.scratch_area;

        let visited = [];
        for (let a of exp.arguments)
            visited.push(this.visitOrNull(a));

        visited.forEach ((a, i) => {
            let gep = ir.createGetElementPointer(spread_alloca, [consts.int32(0), consts.int64(i)], `spread_gep_${i}`);
            ir.createStore(visited[i], gep, `spread[${i}]-store`);
        });

        let argsCast = ir.createGetElementPointer(spread_alloca, [consts.int32(0), consts.int64(0)], "spread_call_args_load");

        let argv = [consts.int32(arg_count), argsCast];
        return this.createCall(this.ejs_runtime.array_from_iterables, argv, "spread_arr");
    }

    handleGetArg (exp) {
        // the intrinsic looks like this:  %getArg(args_index, default_value)
        //
        // if (argc > args_index) {
        //   result = default_value
        // }
        // else {
        //   if (default_value === undefined) {
        //     result = default_value
        //   }
        //   else {
        //     if (args[args_index] === undefined) {
        //       result = default_value
        //     }
        //     else {
        //       result = args[args_index]
        //     }
        //   }
        // }
        //
        // this expanded form is only necessary when default_value is not undefined (something we know at compile time).
        // when default_value is undefined, we end up with this simpler form:
        //
        // if (argc > args_index) {
        //   result = default_value
        // }
        // else {
        //   result = args[args_index]
        // }

        let arg_num = exp.arguments[0].value;
        let load_argc = this.createLoad(this.currentFunction.topScope.get("%argc"), "argc_n_load");
        let cmp = ir.createICmpUGE(load_argc, consts.int32(arg_num+1), "argcmpresult");
                
        let has_slot_bb = new llvm.BasicBlock(`has_${arg_num}_slot`, this.currentFunction);
        let no_slot_bb   = new llvm.BasicBlock(`no_${arg_num}_slot`, this.currentFunction);
        let merge_bb    = new llvm.BasicBlock(`arg_${arg_num}_merge`, this.currentFunction);

        let arg_value_alloca = this.createAlloca(this.currentFunction, types.EjsValue, `arg_${arg_num}_value`);

        ir.createCondBr(cmp, has_slot_bb, no_slot_bb);
        
        this.doInsideBBlock(has_slot_bb, () => {
            let load_args = this.createLoad(this.currentFunction.topScope.get("%args"), "args_load");
            let arg_ptr = ir.createGetElementPointer(load_args, [consts.int32(arg_num)], `arg${arg_num}_ptr`);
            let arg_load = this.createLoad(arg_ptr, `arg${arg_num}`);
            if (exp.arguments.length > 1 && (exp.arguments[1].type !== b.Literal || exp.arguments[1].value !== undefined)) {
                // more complicated form, we need to check if the passed arg was undefined
                let arg_is_undefined = this.isUndefined(arg_load);
                let arg_select = ir.createSelect(arg_is_undefined, this.visit(exp.arguments[1]), arg_load);
                ir.createStore(arg_select, arg_value_alloca, "store_arg_value");
            }
            else {
                // simplified case above, just store it into our alloca
                ir.createStore(arg_load, arg_value_alloca, "store_arg_value");
            }
            ir.createBr(merge_bb);
        });

        this.doInsideBBlock(no_slot_bb, () => {
            // we didn't have the slot
            let default_arg = this.visit(exp.arguments[1]);
            ir.createStore(default_arg, arg_value_alloca, "store_arg_value");
            ir.createBr(merge_bb);
        });

        ir.setInsertPoint(merge_bb);
        return ir.createLoad(arg_value_alloca, "load_arg_value");
    }

    handleCreateIterResult (exp) {
        let value = this.visit(exp.arguments[0]);
        let done = this.visit(exp.arguments[1]);
        return this.createCall(this.ejs_runtime.create_iter_result, [value, done], "iter_result");
    }

    handleCreateIteratorWrapper (exp) {
        let iter = this.visit(exp.arguments[0]);
        return this.createCall(this.ejs_runtime.iterator_wrapper_new, [iter], "iter_wrapper");
    }
}

class AddFunctionsVisitor extends TreeVisitor {
    constructor (module, abi, dibuilder, difile) {
        super();
        this.module = module;
        this.abi = abi;
        this.dibuilder = dibuilder;
        this.difile = difile;
    }

    visitFunction (n) {
        if (n && n.id && n.id.name)
            n.ir_name = n.id.name;
        else
            n.ir_name = "_ejs_anonymous";

        // at this point point n.params includes %env as its first param, and is followed by all the formal parameters from the original
        // script source.  we remove the %env parameter and save off he rest of the formal parameter names, and replace the list with
        // our runtime parameters.

        // remove %env from the formal parameter list, but save its name first
        let env_name = n.params[0].name;
        n.params.splice(0, 1);
        // and store the JS formal parameters someplace else
        n.formal_params = n.params;

        n.params = [];
        for (let param of this.abi.ejs_params)
            n.params.push ({ type: b.Identifier, name: param.name, llvm_type: param.llvm_type });
        n.params[this.abi.env_param_index].name = env_name;

        // create the llvm IR function using our platform calling convention
        n.ir_func = types.takes_builtins(this.abi.createFunction(this.module, n.ir_name, this.abi.ejs_return_type, n.params.map ( (param) => param.llvm_type )));
        if (!n.toplevel) n.ir_func.setInternalLinkage();

        let lineno = 0;
        let col = 0;
        if (n.loc) {
            lineno = n.loc.start.line;
            col = n.loc.start.column;
        }
        if (this.dibuilder && this.difile)
            n.ir_func.debug_info = this.dibuilder.createFunction(this.difile, n.ir_name, n.displayName || n.ir_name, this.difile, lineno, false, true, lineno, 0, true, n.ir_func);

        let ir_args = n.ir_func.args;
        n.params.forEach( (param, i) => {
            ir_args[i].setName(param.name);
        });

        // we don't need to recurse here since we won't have nested functions at this point
        return n;
    }
}

function sanitize_with_regexp (filename) {
    return filename.replace(/[.,-\/\\]/g, "_"); // this is insanely inadequate
}

function insert_toplevel_func (tree, moduleInfo) {
    let toplevel = {
        type: b.FunctionDeclaration,
        id:   b.identifier(moduleInfo.toplevel_function_name),
        displayName: "toplevel",
        params: [],
        defaults: [],
        body: {
            type: b.BlockStatement,
            body: tree.body,
            loc: {
                start: {
                    line: 0,
                    column: 0
                }
            }
        },
        toplevel: true,
        loc: {
            start: {
                line: 0,
                column: 0
            }
        }
    };

    tree.body = [toplevel];
    return tree;
}

export function compile (tree, base_output_filename, source_filename, module_infos, options) {
    let abi = (options.target_arch === "armv7" || options.target_arch === "armv7s"/* || options.target_arch === "x86"*/) ? new SRetABI() : new ABI();

    types.initTypes(options.target_pointer_size === 32);

    let module_filename = source_filename;

    if (module_filename.endsWith(".js"))
        module_filename = module_filename.substring(0, module_filename.length-3);

    let this_module_info = module_infos.get(module_filename);

    tree = insert_toplevel_func(tree, this_module_info);

    debug.log ( () => escodegenerate(tree) );

    let toplevel_name = tree.body[0].id.name;
    
    //debug.log 1, "before closure conversion"
    //debug.log 1, -> escodegenerate tree

    tree = closure_convert(tree, source_filename, module_infos, options);

    debug.log (1, "after closure conversion" );
    debug.log (1, () => escodegenerate(tree) );

    /*
     tree = typeinfer.run tree

     debug.log 1, "after type inference"
     debug.log 1, -> escodegenerate tree
     */
    tree = optimizations.run(tree);
    
    debug.log (1, "after optimization" );
    debug.log (1, () => escodegenerate(tree) );

    let module = new llvm.Module(base_output_filename);
    
    module.toplevel_name = toplevel_name;

    let module_accessors = [];
    this_module_info.exports.forEach ((export_info, key) => {
        let module_prop = undefined;
        let f = this_module_info.getExportGetter(key);
        if (f) {
            if (!module_prop) module_prop = { key };
            module_prop.getter = f;
            tree.body.push(f);
        }
        f = this_module_info.getExportSetter(key);
        if (f) {
            if (!module_prop) module_prop = { key };
            module_prop.setter = f;
            tree.body.push(f);
        }
        if (module_prop)
            module_accessors.push(module_prop);
    });

    let dibuilder;
    let difile;

    if (options.debug) {
        dibuilder = new llvm.DIBuilder(module);
        difile = dibuilder.createFile(source_filename + ".js", process.cwd());

        dibuilder.createCompileUnit(source_filename + ".js", process.cwd(), "ejs", true, "", 2);
    }

    let visitor = new AddFunctionsVisitor(module, abi, dibuilder, difile);

    tree = visitor.visit(tree);

    debug.log ( () => escodegenerate(tree) );

    visitor = new LLVMIRVisitor(module, source_filename, options, abi, module_infos, this_module_info, dibuilder, difile);

    if (options.debug)
        dibuilder.finalize();

    visitor.emitModuleInfo();

    visitor.visit(tree);

    visitor.emitModuleResolution(module_accessors);

    return module;
}
