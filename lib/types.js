let llvm = require('llvm');

export let string      = llvm.Type.getInt8Ty().pointerTo();
export let int8Pointer = string;
export let bool        = llvm.Type.getInt8Ty();
export let voidTy      = llvm.Type.getVoidTy();
export let jschar      = llvm.Type.getInt16Ty();
export let int1        = llvm.Type.getInt1Ty();
export let int32       = llvm.Type.getInt32Ty();
export let int64       = llvm.Type.getInt64Ty();
export let double      = llvm.Type.getDoubleTy();

export let EjsValueLayout = llvm.StructType.create("EjsValueType", [int64]);
export let EjsValue = EjsValueLayout;

export let EjsClosureEnv   = llvm.StructType.create("struct.EJSClosureEnv", [int32, int32, llvm.ArrayType.get(EjsValueLayout, 1)]);
export let EjsPropIterator = EjsValue;
//export let EjsClosureFunc  = llvm.FunctionType.get(EjsValue, [EjsValue, EjsValue, int32, EjsValue.pointerTo()]).pointerTo();
export let EjsClosureFunc  = llvm.FunctionType.get(voidTy, [EjsValue.pointerTo(), EjsValue, EjsValue, int32, EjsValue.pointerTo()]).pointerTo();
export let getEjsClosureFunc = (abi) => abi.createFunctionType(EjsValue, [EjsValue, EjsValue, int32, EjsValue.pointerTo()]).pointerTo();
        
export let EjsPrimString   = llvm.StructType.create("EjsPrimString", [int32, int32, int64, int64 ]); // XXX not the real structure but it should be good

export let EjsSpecops      = llvm.StructType.create("struct.EJSSpecOps", []); // XXX

export let EjsPropertyMap  = llvm.StructType.create("struct.EJSPropertyMap", [
        jschar.pointerTo(),               // _EJSPropertyMapSlot** slots
        jschar.pointerTo(),               // _EJSPropertyMapSlot* first_insert
        jschar.pointerTo(),               // _EJSPropertyMapSlot* last_insert
        int32,                            // int nslots;
        int32                             // int inuse;
]);
        
export let EjsObject = llvm.StructType.create("struct.EJSObject", [
        string,                 // GCObjectHeader gc_header;
        EjsSpecops.pointerTo(), // EJSSpecOps*    ops;
        EjsValue,               // ejsval         proto; // the __proto__ property
        EjsPropertyMap.pointerTo(),  // EJSPropertyMap map;
]);

export let EjsFunction = llvm.StructType.create("struct.EJSFunction", [
        EjsObject,               // EJSObject obj;
        EjsClosureFunc,        // EJSClosureFunc func;
        EjsValue,              // ejsval   env;

        int32,                   // EJSBool  bound;
]);

// exception types

// the c++ typeinfo for our exceptions
export let EjsExceptionTypeInfo = llvm.StructType.create("EjsExceptionTypeInfoType", [int8Pointer, int8Pointer, int8Pointer]).pointerTo();

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
