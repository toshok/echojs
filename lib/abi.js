/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import * as llvm   from '@llvm';
import * as types  from './types';
import * as consts from './consts';

let ir = llvm.IRBuilder;

// our base ABI class assumes that there are no restrictions on
// EjsValue types, and that they can be passed by value and returned by
// value with no modification to signatures or callsites.
//
export class ABI {
    constructor () {
        this.ejs_return_type = types.EjsValue;
        this.ejs_params = [
            { name: "%env",     llvm_type: types.EjsValue }, // should be EjsClosureEnv
            { name: "%this",    llvm_type: types.EjsValue },
            { name: "%argc",    llvm_type: types.Int32 },
            { name: "%args",    llvm_type: types.EjsValue.pointerTo() }
        ];
        this.env_param_index  = 0;
        this.this_param_index = 1;
        this.argc_param_index = 2;
        this.args_param_index = 3;
    }

    // this function c&p from LLVMIRVisitor below
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
    forwardCalleeAttributes (fromCallee, toCall) {
        if (fromCallee.doesNotThrow) toCall.setDoesNotThrow();
        if (fromCallee.doesNotAccessMemory) toCall.setDoesNotAccessMemory();
        if (!fromCallee.doesNotAccessMemory && fromCallee.onlyReadsMemory) toCall.setOnlyReadsMemory();
        toCall._ejs_returns_ejsval_bool = fromCallee.returns_ejsval_bool;
    }
    
    createCall (fromFunction, callee, argv, callname) { return ir.createCall(callee, argv, callname); }
    createInvoke (fromFunction, callee, argv, normal_block, exc_block, callname) { return ir.createInvoke(callee, argv, normal_block, exc_block, callname); }
    createRet (fromFunction, value) { return ir.createRet(value); }
    createExternalFunction (inModule, name, ret_type, param_types) { return inModule.getOrInsertExternalFunction(name, ret_type, param_types); }
    createFunction (inModule, name, ret_type, param_types) { return inModule.getOrInsertFunction(name, ret_type, param_types); }
    createFunctionType (ret_type, param_types) { return llvm.FunctionType.get(ret_type, param_types); }
}
