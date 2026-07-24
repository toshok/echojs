/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

import * as llvm from "@llvm";

export const String = llvm.Type.getInt8Ty().pointerTo();
export const Int8Pointer = String;
export const Bool = llvm.Type.getInt8Ty();
export const Void = llvm.Type.getVoidTy();
export const JSChar = llvm.Type.getInt16Ty();
export const Int1 = llvm.Type.getInt1Ty();
export const Int32 = llvm.Type.getInt32Ty();
export const Int64 = llvm.Type.getInt64Ty();
export const Double = llvm.Type.getDoubleTy();

export const EjsLandingPad = llvm.StructType.create("EjsLandingPad", [Int8Pointer, Int32]);
export const EjsValueLayout = llvm.StructType.create("EjsValueType", [Int64]);
export const EjsValue: llvm.Type = EjsValueLayout;

export const EjsClosureEnv = llvm.StructType.create("struct.EJSClosureEnv", [
    Int32, // GCObjectHeader gc_header (low half)
    Int32, // GCObjectHeader shape/gc bits (high half)
    Int32, // uint32_t length
    Int32, // padding (slots are 8-aligned)
    llvm.ArrayType.get(EjsValueLayout, 1),
]);
export const EjsPropIterator = EjsValue;
export const EjsClosureFunc = llvm.FunctionType.get(
    Void,
    [EjsValue.pointerTo(), EjsValue, EjsValue.pointerTo(), Int32, EjsValue.pointerTo(), Int32],
    EjsValue
).pointerTo();

// the piece of the ABI interface this module needs (abi.ts imports this
// module, so the full type would be a cycle)
export interface FunctionTypeMaker {
    createFunctionType(ret: llvm.Type, params: llvm.Type[]): llvm.FunctionType;
}

export const getEjsClosureFunc = (abi: FunctionTypeMaker): llvm.Type =>
    abi
        .createFunctionType(EjsValue, [
            EjsValue,
            EjsValue.pointerTo(),
            Int32,
            EjsValue.pointerTo(),
            EjsValue,
        ])
        .pointerTo();

// {u64 gc_header, u32 length, i32 hash, 8-byte data union} — matches the
// runtime's 24-byte _EJSPrimString; only the size matters here (globals of
// this type are zero-initialized and filled by _ejs_string_init_literal)
export const EjsPrimString = llvm.StructType.create("EjsPrimString", [
    Int32,
    Int32,
    Int32,
    Int32,
    Int64,
]);

export const EjsSpecops = llvm.StructType.create("struct.EJSSpecOps", []); // XXX

export const EjsPropertyMap = llvm.StructType.create("struct.EJSPropertyMap", [
    JSChar.pointerTo(), // _EJSPropertyMapSlot** slots
    JSChar.pointerTo(), // _EJSPropertyMapSlot* first_insert
    JSChar.pointerTo(), // _EJSPropertyMapSlot* last_insert
    Int32, // int nslots;
    Int32, // int inuse;
]);

// initialized by initTypes() once the target's pointer size is known;
// reading them before that is a bug (they trap as undefined at runtime)
export let EjsObject: llvm.StructType;
export let EjsFunction: llvm.StructType;
export let EjsModule: llvm.StructType;

function CreateModuleTy(suffix: string, num_exports: number): llvm.StructType {
    return llvm.StructType.create(`struct.EJSModule${suffix}`, [
        EjsObject, // EJSObject obj;
        String, // const char* module_name
        Int32, // int32_t num_exports
        llvm.ArrayType.get(EjsValueLayout, num_exports),
    ]);
}

export function getModuleSpecificType(module_name: string, num_exports: number): llvm.StructType {
    return CreateModuleTy(`_${module_name}`, num_exports);
}

export function initTypes(is32bit: boolean): void {
    // EJSObject's struct type depends on the pointer size of the
    // architecture.  on 32 bit platforms (XXX or maybe just x86?)
    // clang inserts 4 bytes of padding at the end.  we therefore need
    // to delay initialization of EJSObject (and therefore its uses)
    // until after we've determined pointer size.

    // the 64-bit GCObjectHeader is represented as two i32s (little-endian
    // halves) so the P4.3 shape-guard emitter can load the shape/gc half
    // (field 1) without masking a 64-bit load; byte layout is identical
    if (is32bit) {
        EjsObject = llvm.StructType.create("struct.EJSObject", [
            Int32, // GCObjectHeader gc_header (low half: scan type, user flags)
            Int32, // GCObjectHeader shape index / gc bits (high half)
            EjsSpecops.pointerTo(), // EJSSpecOps*    ops;
            EjsValue, // ejsval         proto; // the __proto__ property
            EjsPropertyMap.pointerTo(), // EJSPropertyMap map;
            llvm.ArrayType.get(llvm.Type.getInt8Ty(), 4), // alignment that clang adds
        ]);
    } else {
        EjsObject = llvm.StructType.create("struct.EJSObject", [
            Int32, // GCObjectHeader gc_header (low half: scan type, user flags)
            Int32, // GCObjectHeader shape index / gc bits (high half)
            EjsSpecops.pointerTo(), // EJSSpecOps*    ops;
            EjsValue, // ejsval         proto; // the __proto__ property
            EjsPropertyMap.pointerTo(), // EJSPropertyMap map;
        ]);
    }

    EjsFunction = llvm.StructType.create("struct.EJSFunction", [
        EjsObject, // EJSObject obj;
        EjsClosureFunc, // EJSClosureFunc func;
        EjsValue, // ejsval   env;

        Int32, // EJSBool  bound;
    ]);

    EjsModule = CreateModuleTy("", 1);
}

// exception types

// the c++ typeinfo for our exceptions
export const EjsExceptionTypeInfo = llvm.StructType.create("EjsExceptionTypeInfoType", [
    Int8Pointer,
    Int8Pointer,
    Int8Pointer,
]).pointerTo();

export function takes_builtins(n: llvm.EjsFunction): llvm.EjsFunction {
    n.takes_builtins = true;
    return n;
}

export function only_reads_memory(n: llvm.EjsFunction): llvm.EjsFunction {
    n.setOnlyReadsMemory();
    return n;
}

export function does_not_access_memory(n: llvm.EjsFunction): llvm.EjsFunction {
    n.setDoesNotAccessMemory();
    return n;
}

export function does_not_throw(n: llvm.EjsFunction): llvm.EjsFunction {
    n.setDoesNotThrow();
    return n;
}

export function returns_ejsval_bool(n: llvm.EjsFunction): llvm.EjsFunction {
    n.returns_ejsval_bool = true;
    return n;
}
