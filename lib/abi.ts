/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

import * as llvm from "@llvm";
import * as types from "./types";

const ir = llvm.IRBuilder;

export interface EjsParam {
    name: string;
    llvm_type: llvm.Type;
}

// our base ABI class assumes that there are no restrictions on
// EjsValue types, and that they can be passed by value and returned by
// value with no modification to signatures or callsites.
//
export class ABI {
    ejs_return_type: llvm.Type = types.EjsValue;
    ejs_params: EjsParam[] = [
        { name: "%env", llvm_type: types.EjsValue }, // should be EjsClosureEnv
        { name: "%this", llvm_type: types.EjsValue.pointerTo() },
        { name: "%argc", llvm_type: types.Int32 },
        { name: "%args", llvm_type: types.EjsValue.pointerTo() },
        { name: "%newTarget", llvm_type: types.EjsValue },
    ];
    env_param_index = 0;
    this_param_index = 1;
    argc_param_index = 2;
    args_param_index = 3;
    newTarget_param_index = 3;

    createAlloca(func: llvm.EjsFunction, type: llvm.Type, name: string): llvm.AllocaInst {
        const saved_insert_point = ir.getInsertBlock();
        ir.setInsertPointStartBB(func.entry_bb!);
        const alloca = ir.createAlloca(type, name);

        // if EjsValue was a pointer value we would be able to use the llvm
        // gcroot intrinsic here.  but with the nan boxing we kinda lose out
        // as the llvm IR code doesn't permit non-reference types to be gc
        // roots.

        ir.setInsertPoint(saved_insert_point);
        return alloca;
    }

    forwardCalleeAttributes(fromCallee: llvm.EjsFunction, toCall: llvm.CallInst): void {
        if (fromCallee.doesNotThrow) toCall.setDoesNotThrow();
        if (fromCallee.doesNotAccessMemory) toCall.setDoesNotAccessMemory();
        if (!fromCallee.doesNotAccessMemory && fromCallee.onlyReadsMemory)
            toCall.setOnlyReadsMemory();
        toCall._ejs_returns_ejsval_bool = fromCallee.returns_ejsval_bool;
    }

    createCall(
        fromFunction: llvm.EjsFunction,
        calleeType: llvm.FunctionType,
        callee: llvm.Value,
        argv: llvm.Value[],
        callname: string
    ): llvm.Value {
        return ir.createCall(calleeType, callee, argv, callname);
    }

    createInvoke(
        fromFunction: llvm.EjsFunction,
        calleeType: llvm.FunctionType,
        callee: llvm.Value,
        argv: llvm.Value[],
        normal_block: llvm.BasicBlock,
        exc_block: llvm.BasicBlock,
        callname: string
    ): llvm.Value {
        return ir.createInvoke(calleeType, callee, argv, normal_block, exc_block, callname);
    }

    createRet(fromFunction: llvm.EjsFunction, value: llvm.Value): llvm.Value {
        return ir.createRet(value);
    }

    createExternalFunction(
        inModule: llvm.Module,
        name: string,
        ret_type: llvm.Type,
        param_types: llvm.Type[]
    ): llvm.EjsFunction {
        return inModule.getOrInsertExternalFunction(name, ret_type, param_types);
    }

    createFunction(
        inModule: llvm.Module,
        name: string,
        ret_type: llvm.Type,
        param_types: llvm.Type[]
    ): llvm.EjsFunction {
        return inModule.getOrInsertFunction(name, ret_type, param_types);
    }

    createFunctionType(ret_type: llvm.Type, param_types: llvm.Type[]): llvm.FunctionType {
        return llvm.FunctionType.get(ret_type, param_types);
    }
}
