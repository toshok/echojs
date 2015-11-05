/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */
import * as llvm from '@llvm';

export let String      = llvm.Type.getInt8Ty().pointerTo();
export let Int8Pointer = String;
export let Bool        = llvm.Type.getInt8Ty();
export let Void        = llvm.Type.getVoidTy();
export let JSChar      = llvm.Type.getInt16Ty();
export let Int1        = llvm.Type.getInt1Ty();
export let Int32       = llvm.Type.getInt32Ty();
export let Int64       = llvm.Type.getInt64Ty();
export let Double      = llvm.Type.getDoubleTy();

export let EjsValueLayout = llvm.StructType.create("EjsValueType", [Int64]);
export let EjsValue = EjsValueLayout;

export let EjsClosureEnv   = llvm.StructType.create("struct.EJSClosureEnv", [Int32, Int32, llvm.ArrayType.get(EjsValueLayout, 1)]);
export let EjsPropIterator = EjsValue;
//export let EjsClosureFunc  = llvm.FunctionType.get(EjsValue, [EjsValue, EjsValue, Int32, EjsValue.pointerTo()]).pointerTo();
export let EjsClosureFunc  = llvm.FunctionType.get(Void, [EjsValue.pointerTo(), EjsValue, EjsValue.pointerTo(), Int32, EjsValue.pointerTo(), Int32], EjsValue).pointerTo();
export let getEjsClosureFunc = (abi) => abi.createFunctionType(EjsValue, [EjsValue, EjsValue.pointerTo(), Int32, EjsValue.pointerTo(), Int32, EjsValue]).pointerTo();

export let EjsPrimString   = llvm.StructType.create("EjsPrimString", [Int32, Int32, Int64, Int64 ]); // XXX not the real structure but it should be good

export let EjsSpecops      = llvm.StructType.create("struct.EJSSpecOps", []); // XXX

export let EjsPropertyMap  = llvm.StructType.create("struct.EJSPropertyMap", [
    JSChar.pointerTo(),               // _EJSPropertyMapSlot** slots
    JSChar.pointerTo(),               // _EJSPropertyMapSlot* first_insert
    JSChar.pointerTo(),               // _EJSPropertyMapSlot* last_insert
    Int32,                            // int nslots;
    Int32                             // int inuse;
]);

export let EjsObject = llvm.StructType.create("struct.EJSObject", [
    String,                 // GCObjectHeader gc_header;
    EjsSpecops.pointerTo(), // EJSSpecOps*    ops;
    EjsValue,               // ejsval         proto; // the __proto__ property
    EjsPropertyMap.pointerTo(),  // EJSPropertyMap map;
]);

export let EjsFunction = llvm.StructType.create("struct.EJSFunction", [
    EjsObject,             // EJSObject obj;
    EjsClosureFunc,        // EJSClosureFunc func;
    EjsValue,              // ejsval   env;

    Int32,                 // EJSBool  bound;
]);

function CreateModuleTy (suffix, num_exports) {
    return llvm.StructType.create(`struct.EJSModule${suffix}`, [
        EjsObject,             // EJSObject obj;
        String,                // const char* module_name
        Int32,                 // int32_t num_exports
        llvm.ArrayType.get(EjsValueLayout, num_exports)
    ]);
}
        
export let EjsModule = CreateModuleTy("", 1);

export function getModuleSpecificType (module_name, num_exports) {
    return CreateModuleTy(`_${module_name}`, num_exports);
}

// exception types

// the c++ typeinfo for our exceptions
export let EjsExceptionTypeInfo = llvm.StructType.create("EjsExceptionTypeInfoType", [Int8Pointer, Int8Pointer, Int8Pointer]).pointerTo();

export function takes_builtins(n) {
    n.takes_builtins = true;
    return n;
}

export function only_reads_memory(n) {
    n.setOnlyReadsMemory();
    return n;
}

export function does_not_access_memory(n) {
    n.setDoesNotAccessMemory();
    return n;
}

export function does_not_throw(n) {
    n.setDoesNotThrow();
    return n;
}

export function returns_ejsval_bool(n) {
    n.returns_ejsval_bool = true;
    return n;
}
