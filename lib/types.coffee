llvm = require 'llvm'

exports.string      = stringTy      = llvm.Type.getInt8Ty().pointerTo()
exports.int8Pointer = int8PointerTy = stringTy
exports.bool        = boolTy        = llvm.Type.getInt8Ty()
exports.void        = voidTy        = llvm.Type.getVoidTy()
exports.jschar      = jscharTy      = llvm.Type.getInt16Ty()
exports.int32       = int32Ty       = llvm.Type.getInt32Ty()
exports.int64       = int64Ty       = llvm.Type.getInt64Ty()
exports.double      = doubleTy      = llvm.Type.getDoubleTy()

exports.EjsValue = EjsValueTy = llvm.StructType.create "EjsValueType", [int64Ty]

exports.EjsClosureEnv   = EjsClosureEnvTy = EjsValueTy
exports.EjsPropIterator = EjsPropIteratorTy = EjsValueTy
exports.EjsClosureFunc  = EjsClosureFuncTy = (llvm.FunctionType.get EjsValueTy, [EjsClosureEnvTy, EjsValueTy, int32Ty, EjsValueTy.pointerTo()]).pointerTo()
exports.EjsPrimString   = EjsPrimStringTy = llvm.StructType.create "EjsPrimString", [int32Ty, int32Ty, int64Ty, int64Ty ] # XXX not the real structure but it should be good

# exception types

# the c++ typeinfo for our exceptions
exports.EjsExceptionTypeInfo = EjsExceptionTypeInfoTy = (llvm.StructType.create "EjsExceptionTypeInfoType", [int8PointerTy, int8PointerTy, int8PointerTy]).pointerTo()

exports.takes_builtins = (n) ->
        n.takes_builtins = true
        n

exports.only_reads_memory = (n) ->
        n.setOnlyReadsMemory()
        n

exports.does_not_access_memory = (n) ->
        n.setDoesNotAccessMemory()
        n

exports.does_not_throw = (n) ->
        n.setDoesNotThrow()
        n

