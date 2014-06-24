llvm = require 'llvm'

exports.string      = stringTy      = llvm.Type.getInt8Ty().pointerTo()
exports.int8Pointer = int8PointerTy = stringTy
exports.bool        = boolTy        = llvm.Type.getInt8Ty()
exports.void        = voidTy        = llvm.Type.getVoidTy()
exports.jschar      = jscharTy      = llvm.Type.getInt16Ty()
exports.int32       = int32Ty       = llvm.Type.getInt32Ty()
exports.int64       = int64Ty       = llvm.Type.getInt64Ty()
exports.double      = doubleTy      = llvm.Type.getDoubleTy()

exports.EjsValueLayout = EjsValueLayoutTy = llvm.StructType.create "EjsValueType", [int64Ty]
exports.EjsValue = EjsValueTy = EjsValueLayoutTy

exports.EjsClosureEnv   = EjsClosureEnvTy = llvm.StructType.create "struct.EJSClosureEnv", [int32Ty, int32Ty, (llvm.ArrayType.get EjsValueLayoutTy, 1)]
exports.EjsPropIterator = EjsPropIteratorTy = EjsValueTy
#exports.EjsClosureFunc  = EjsClosureFuncTy = (llvm.FunctionType.get EjsValueTy, [EjsValueTy, EjsValueTy, int32Ty, EjsValueTy.pointerTo()]).pointerTo()
exports.EjsClosureFunc  = EjsClosureFuncTy = (llvm.FunctionType.get voidTy, [EjsValueTy.pointerTo(), EjsValueTy, EjsValueTy, int32Ty, EjsValueTy.pointerTo()]).pointerTo()
exports.getEjsClosureFunc = (abi) -> abi.createFunctionType(EjsValueTy, [EjsValueTy, EjsValueTy, int32Ty, EjsValueTy.pointerTo()]).pointerTo()
        
exports.EjsPrimString   = EjsPrimStringTy = llvm.StructType.create "EjsPrimString", [int32Ty, int32Ty, int64Ty, int64Ty ] # XXX not the real structure but it should be good

exports.EjsSpecops      = EjsSpecopsTy = llvm.StructType.create "struct.EJSSpecOps", [] # XXX

exports.EjsPropertyMap  = EjsPropertyMapTy = llvm.StructType.create "struct.EJSPropertyMap", [
        jscharTy.pointerTo(),               # _EJSPropertyMapSlot** slots
        jscharTy.pointerTo(),               # _EJSPropertyMapSlot* first_insert
        jscharTy.pointerTo(),               # _EJSPropertyMapSlot* last_insert
        int32Ty,                            # int nslots;
        int32Ty                             # int inuse;
]
        
exports.EjsObject = EjsObjectTy = llvm.StructType.create "struct.EJSObject", [
        stringTy,                 # GCObjectHeader gc_header;
        EjsSpecopsTy.pointerTo(), # EJSSpecOps*    ops;
        EjsValueTy,               # ejsval         proto; // the __proto__ property
        EjsPropertyMapTy.pointerTo(),  # EJSPropertyMap map;
]

exports.EjsFunction = EjsFunctionTy = llvm.StructType.create "struct.EJSFunction", [
        EjsObjectTy,             # EJSObject obj;
        EjsClosureFuncTy,        # EJSClosureFunc func;
        EjsValueTy,              # ejsval   env;

        int32Ty,                 # EJSBool  bound;
]

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

exports.returns_ejsval_bool = (n) ->
        n.returns_ejsval_bool = true
        n
