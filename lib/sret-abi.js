/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import * as llvm   from '@llvm';
import * as types  from './types';
import * as consts from './consts';
import { ABI } from './abi';

let ir = llvm.IRBuilder;

// armv7/x86 requires us to pass a pointer to a stack slot for the return value when it's EjsValue.
// so functions that would normally be defined as:
// 
//   ejsval _ejs_normal_func (ejsval env, ejsval this, uint32_t argc, ejsval* args)
//
// are instead expressed as:
// 
//   void _ejs_sret_func (ejsval* sret, ejsval env, ejsval this, uint32_t argc, ejsval* args)
// 
export class SRetABI extends ABI {
    constructor () {
        super();
        this.ejs_return_type = types.Void;
        this.ejs_params.unshift({ name: "%retval", llvm_type: types.EjsValue.pointerTo() });
        this.env_param_index += 1;
        this.this_param_index += 1;
        this.argc_param_index += 1;
        this.args_param_index += 1;
        this.callFlags_param_index += 1;
        this.newTarget_param_index += 1;
    }
    
    createCall (fromFunction, callee, argv, callname) {
        if (callee.hasStructRetAttr()) {
            let sret_alloca = this.createAlloca(fromFunction, types.EjsValue, "sret");
            argv.unshift(sret_alloca);

            //sret_as_i8 = ir.createBitCast sret_alloca, types.Int8Pointer, "sret_as_i8"
            //ir.createLifetimeStart sret_as_i8, consts.int64(8) //sizeof(ejsval)
            let call = super.createCall(fromFunction, callee, argv, "");
            call.setStructRet();

            let rv = ir.createLoad(sret_alloca, callname);
            //ir.createLifetimeEnd sret_as_i8, consts.int64(8) //sizeof(ejsval)
            return rv;
        }
        else {
            return super.createCall(fromFunction, callee, argv, callname);
        }
    }

    createInvoke (fromFunction, callee, argv, normal_block, exc_block, callname) {
        if (callee.hasStructRetAttr()) {
            let sret_alloca = this.createAlloca(fromFunction, types.EjsValue, "sret");
            argv.unshift(sret_alloca);

            //sret_as_i8 = ir.createBitCast sret_alloca, types.Int8Pointer, "sret_as_i8"
            //ir.createLifetimeStart sret_as_i8, consts.int64(8) //sizeof(ejsval)
            let call = super.createInvoke(fromFunction, callee, argv, normal_block, exc_block, "");
            call.setStructRet();

            ir.setInsertPoint(normal_block);
            let rv = ir.createLoad(sret_alloca, callname);
            //ir.createLifetimeEnd sret_as_i8, consts.int64(8) //sizeof(ejsval)
            return rv;
        }
        else {
            return super.createInvoke(fromFunction, callee, argv, normal_block, exc_block, callname);
        }
    }

    createRet (fromFunction, value) {
        ir.createStore(value, fromFunction.args[0]);
        return ir.createRetVoid();
    }
    
    createExternalFunction (inModule, name, ret_type, param_types) {
        return this.createFunction(inModule, name, ret_type, param_types, true);
    }
    
    createFunction (inModule, name, ret_type, param_types, external = false) {
        let sret = false;
        let rv;
        if (ret_type === types.EjsValue) {
            param_types.unshift(ret_type.pointerTo());
            ret_type = types.Void;
            sret = true;
        }
        if (external)
            rv = inModule.getOrInsertExternalFunction(name, ret_type, param_types);
        else
            rv = inModule.getOrInsertFunction(name, ret_type, param_types);

        if (sret) rv.setStructRet();
        return rv;
    }

    createFunctionType (ret_type, param_types) {
        if (ret_type === types.EjsValue) {
            param_types.unshift(ret_type.pointerTo());
            ret_type = types.Void;
        }
        return super.createFunctionType(ret_type, param_types);
    }
}

