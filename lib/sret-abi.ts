/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

import * as llvm from "@llvm";
import * as types from "./types";
import { ABI } from "./abi";

const ir = llvm.IRBuilder;

// armv7/x86 requires us to pass a pointer to a stack slot for the return
// value when it's EjsValue.  so functions that would normally be defined
// as:
//
//   ejsval _ejs_normal_func (ejsval env, ejsval this, uint32_t argc, ejsval* args)
//
// are instead expressed as:
//
//   void _ejs_sret_func (ejsval* sret, ejsval env, ejsval this, uint32_t argc, ejsval* args)
//
export class SRetABI extends ABI {
    constructor() {
        super();
        this.ejs_return_type = types.Void;
        this.ejs_params.unshift({
            name: "%retval",
            llvm_type: types.EjsValue.pointerTo(),
        });
        this.env_param_index += 1;
        this.this_param_index += 1;
        this.argc_param_index += 1;
        this.args_param_index += 1;
        this.newTarget_param_index += 1;
    }

    override createCall(
        fromFunction: llvm.EjsFunction,
        calleeType: llvm.FunctionType,
        callee: llvm.Value,
        argv: llvm.Value[],
        callname: string
    ): llvm.Value {
        if (calleeHasStructRet(callee)) {
            const sret_alloca = this.createAlloca(fromFunction, types.EjsValue, "sret");
            argv.unshift(sret_alloca);

            const call = super.createCall(fromFunction, calleeType, callee, argv, "");
            (call as llvm.CallInst).setStructRet();

            return ir.createLoad(types.EjsValue, sret_alloca, callname);
        }
        return super.createCall(fromFunction, calleeType, callee, argv, callname);
    }

    override createInvoke(
        fromFunction: llvm.EjsFunction,
        calleeType: llvm.FunctionType,
        callee: llvm.Value,
        argv: llvm.Value[],
        normal_block: llvm.BasicBlock,
        exc_block: llvm.BasicBlock,
        callname: string
    ): llvm.Value {
        if (calleeHasStructRet(callee)) {
            const sret_alloca = this.createAlloca(fromFunction, types.EjsValue, "sret");
            argv.unshift(sret_alloca);

            const call = super.createInvoke(
                fromFunction,
                calleeType,
                callee,
                argv,
                normal_block,
                exc_block,
                ""
            );
            (call as llvm.CallInst).setStructRet();

            ir.setInsertPoint(normal_block);
            return ir.createLoad(types.EjsValue, sret_alloca, callname);
        }
        return super.createInvoke(
            fromFunction,
            calleeType,
            callee,
            argv,
            normal_block,
            exc_block,
            callname
        );
    }

    override createRet(fromFunction: llvm.EjsFunction, value: llvm.Value): llvm.Value {
        ir.createStore(value, fromFunction.args[0]!);
        return ir.createRetVoid();
    }

    override createExternalFunction(
        inModule: llvm.Module,
        name: string,
        ret_type: llvm.Type,
        param_types: llvm.Type[]
    ): llvm.EjsFunction {
        return this.createFunction(inModule, name, ret_type, param_types, true);
    }

    override createFunction(
        inModule: llvm.Module,
        name: string,
        ret_type: llvm.Type,
        param_types: llvm.Type[],
        external = false
    ): llvm.EjsFunction {
        let sret = false;
        if (ret_type === types.EjsValue) {
            param_types.unshift(ret_type.pointerTo());
            ret_type = types.Void;
            sret = true;
        }
        const rv = external
            ? inModule.getOrInsertExternalFunction(name, ret_type, param_types)
            : inModule.getOrInsertFunction(name, ret_type, param_types);

        if (sret) rv.setStructRet();
        return rv;
    }

    override createFunctionType(ret_type: llvm.Type, param_types: llvm.Type[]): llvm.FunctionType {
        if (ret_type === types.EjsValue) {
            param_types.unshift(ret_type.pointerTo());
            ret_type = types.Void;
        }
        return super.createFunctionType(ret_type, param_types);
    }
}

// callees arrive as plain Values (function pointers or functions); only
// actual functions carry the sret attribute
function calleeHasStructRet(callee: llvm.Value): callee is llvm.EjsFunction {
    return (
        typeof (callee as llvm.EjsFunction).hasStructRetAttr === "function" &&
        (callee as llvm.EjsFunction).hasStructRetAttr()
    );
}
