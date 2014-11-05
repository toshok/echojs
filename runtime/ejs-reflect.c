/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-function.h"
#include "ejs-error.h"
#include "ejs-math.h"
#include "ejs-string.h"
#include "ejs-symbol.h"
#include "ejs-proxy.h"
#include "ejs-array.h"

ejsval _ejs_Reflect EJSVAL_ALIGNMENT;

// ECMA262: 26.1.1 Reflect.apply ( target, thisArgument, argumentsList ) 
static ejsval
_ejs_Reflect_apply (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval target = _ejs_undefined;
    ejsval thisArgument = _ejs_undefined;
    ejsval argumentsList = _ejs_undefined;

    if (argc > 0) target        = args[0];
    if (argc > 1) thisArgument  = args[1];
    if (argc > 2) argumentsList = args[2];

    // 1. Let obj be ToObject(target). 
    // 2. ReturnIfAbrupt(obj). 
    ejsval obj = ToObject(target);

    // 3. If IsCallable(obj) is false, then throw a TypeError exception. 
    if (!EJSVAL_IS_CALLABLE(obj))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "target is not callable");
        
    // 4. Let args be CreateListFromArray (argumentsList). 
    // 5. ReturnIfAbrupt(args).

    if (!EJSVAL_IS_ARRAY(argumentsList))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "argumentsList is not an array");

    // sparse arrays kinda suck here...
    if (!EJSVAL_IS_DENSE_ARRAY(argumentsList))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "argumentsList is not a dense array");
        
    // 6. Perform the PrepareForTailCall abstract operation. 

    // 7. Return the result of calling the [[Call]] internal method of obj with arguments thisArgument and args.
    return _ejs_invoke_closure(obj, thisArgument, EJS_ARRAY_LEN(argumentsList), EJS_DENSE_ARRAY_ELEMENTS(argumentsList));
}

static ejsval
_ejs_Reflect_construct (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_Reflect_defineProperty (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_Reflect_deleteProperty (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_Reflect_enumerate (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

// ECMA262: 26.1.6 Reflect.get ( target, propertyKey [ , receiver ]) 
static ejsval
_ejs_Reflect_get (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval target = _ejs_undefined;
    ejsval propertyKey = _ejs_undefined;
    ejsval receiver = _ejs_undefined;

    if (argc > 0) target       = args[0];
    if (argc > 1) propertyKey  = args[1];
    if (argc > 2) receiver     = args[2];

    // 1. Let obj be ToObject(target). 
    // 2. ReturnIfAbrupt(obj). 
    ejsval obj = ToObject(target);

    // 3. Let key be ToPropertyKey(propertyKey). 
    // 4. ReturnIfAbrupt(key). 
    ejsval key = ToPropertyKey(propertyKey);

    // 5. If receiver is not present, then 
    //    a. Let receiver be target. 
    if (argc <= 2)
        receiver = target;

    // 6. Return the result of calling the [[Get]] internal method of obj with arguments key, and receiver
    return OP(EJSVAL_TO_OBJECT(obj),Get)(obj, key, receiver);
}

// ECMA262: 26.1.7 Reflect.getOwnPropertyDescriptor ( target, propertyKey )
static ejsval
_ejs_Reflect_getOwnPropertyDescriptor (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval target = _ejs_undefined;
    ejsval propertyKey = _ejs_undefined;
    if (argc > 0) target = args[0];
    if (argc > 1) propertyKey = args[1];

    // 1. Let obj be ToObject(target). 
    // 2. ReturnIfAbrupt(obj). 
    ejsval obj = ToObject(target);

    // 3. Let key be ToPropertyKey(propertyKey). 
    // 4. ReturnIfAbrupt(key). 
    ejsval key = ToPropertyKey(propertyKey);

    // 5. Let desc be the result of calling the [[GetOwnProperty]] internal method of obj with argument key. 
    // 6. ReturnIfAbrupt(desc). 
    EJSPropertyDesc* desc = OP(EJSVAL_TO_OBJECT(obj),GetOwnProperty)(obj, key, NULL);

    // 7. Return the result of calling FromPropertyDescriptor(desc). 
    return FromPropertyDescriptor(desc);
}

// ECMA262: 26.1.8 Reflect.getPrototypeOf ( target ) 
static ejsval
_ejs_Reflect_getPrototypeOf (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval target = _ejs_undefined;
    if (argc > 0) target = args[0];

    // 1. Let obj be ToObject(target). 
    // 2. ReturnIfAbrupt(obj). 
    ejsval obj = ToObject(target);

    // 3. Return the result of calling the [[GetPrototypeOf]] internal method of obj. 
    return OP(EJSVAL_TO_OBJECT(obj),GetPrototypeOf)(obj);
}

// ECMA262: 26.1.9 Reflect.has ( target, propertyKey ) 
static ejsval
_ejs_Reflect_has (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval target = _ejs_undefined;
    ejsval propertyKey = _ejs_undefined;

    if (argc > 0) target       = args[0];
    if (argc > 1) propertyKey  = args[1];

    // 1. Let obj be ToObject(target). 
    // 2. ReturnIfAbrupt(obj). 
    ejsval obj = ToObject(target);

    // 3. Let key be ToPropertyKey(propertyKey). 
    // 4. ReturnIfAbrupt(key). 
    ejsval key = ToPropertyKey(propertyKey);
    
    // 5. Return the result of calling the [[HasProperty]] internal method of obj with argument key.
    return BOOLEAN_TO_EJSVAL(OP(EJSVAL_TO_OBJECT(obj),HasProperty)(obj, key));
}

// ECMA262: 26.1.10 Reflect.isExtensible (target) 
static ejsval
_ejs_Reflect_isExtensible (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval target = _ejs_undefined;
    if (argc > 0) target = args[0];

    // 1. Let obj be ToObject(target). 
    // 2. ReturnIfAbrupt(obj). 
    ejsval obj = ToObject(target);

    // 3. Return the result of calling the [[IsExtensible]] internal method of obj.
    return BOOLEAN_TO_EJSVAL(EJS_OBJECT_IS_EXTENSIBLE(EJSVAL_TO_OBJECT(obj)));
}

// ECMA262: 26.1.11 Reflect.ownKeys ( target ) 
static ejsval
_ejs_Reflect_ownKeys (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
#if notyet
    ejsval target = _ejs_undefined;
    if (argc > 0) target = args[0];

    // 1. Let obj be ToObject(target). 
    // 2. ReturnIfAbrupt(obj). 
    ejsval obj = ToObject(target);
#endif

    // 3. Return the result of calling the [[OwnPropertyKeys]] internal method of obj.
    EJS_NOT_IMPLEMENTED();
}

// ECMA262: 26.1.12 Reflect.preventExtensions ( target ) 
static ejsval
_ejs_Reflect_preventExtensions (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
#if notyet
    ejsval target = _ejs_undefined;
    if (argc > 0) target = args[0];

    // 1. Let obj be ToObject(target). 
    // 2. ReturnIfAbrupt(obj). 
    ejsval obj = ToObject(target);
#endif

    // 3. Return the result of calling the [[PreventExtensions]] internal method of obj.
    EJS_NOT_IMPLEMENTED();
}

// ECMA262: 26.1.13 Reflect.set ( target, propertyKey, V [ , receiver ] ) 
static ejsval
_ejs_Reflect_set (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval target      = _ejs_undefined;
    ejsval propertyKey = _ejs_undefined;
    ejsval V           = _ejs_undefined;
    ejsval receiver    = _ejs_undefined;

    if (argc > 0) target       = args[0];
    if (argc > 1) propertyKey  = args[1];
    if (argc > 2) V            = args[2];
    if (argc > 3) receiver     = args[3];

    // 1. Let obj be ToObject(target). 
    // 2. ReturnIfAbrupt(obj). 
    ejsval obj = ToObject(target);

    // 3. Let key be ToPropertyKey(propertyKey). 
    // 4. ReturnIfAbrupt(key). 
    ejsval key = ToPropertyKey(propertyKey);

    // 5. If receiver is not present, then 
    //    a. Let receiver be target. 
    if (argc <= 3)
        receiver = target;

    // 6. Return the result of calling the [[Set]] internal method of obj with arguments key, V, and receiver. 
    return BOOLEAN_TO_EJSVAL(OP(EJSVAL_TO_OBJECT(obj),Set)(obj, key, V, receiver));
}

// ECMA262: 26.1.14 Reflect.setPrototypeOf ( target, proto ) 
static ejsval
_ejs_Reflect_setPrototypeOf (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval target = _ejs_undefined;
    ejsval proto = _ejs_undefined;

    if (argc > 0) target = args[0];
    if (argc > 1) proto  = args[1];

    // 1. Let obj be ToObject(target). 
    // 2. ReturnIfAbrupt(obj). 
    ejsval obj = ToObject(target);

    // 3. If Type(proto) is not Object and proto is not null, then throw a TypeError exception 
    if (!EJSVAL_IS_OBJECT(proto) && !EJSVAL_IS_NULL(proto))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "prototype argument must be an object or null");

    // 4. Return the result of calling the [[SetPrototypeOf]] internal method of obj with argument proto. 
    return BOOLEAN_TO_EJSVAL(OP(EJSVAL_TO_OBJECT(obj), SetPrototypeOf)(target, proto));
}

void
_ejs_reflect_init(ejsval global)
{
    _ejs_Reflect = _ejs_object_new (_ejs_Object_prototype, &_ejs_Object_specops);
    _ejs_object_setprop (global, _ejs_atom_Reflect, _ejs_Reflect);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Reflect, x, _ejs_Reflect_##x)

    OBJ_METHOD(apply);
    OBJ_METHOD(construct);
    OBJ_METHOD(defineProperty);
    OBJ_METHOD(deleteProperty);
    OBJ_METHOD(enumerate);
    OBJ_METHOD(get);
    OBJ_METHOD(getOwnPropertyDescriptor);
    OBJ_METHOD(getPrototypeOf);
    OBJ_METHOD(has);
    OBJ_METHOD(isExtensible);
    OBJ_METHOD(ownKeys);
    OBJ_METHOD(preventExtensions);
    OBJ_METHOD(set);
    OBJ_METHOD(setPrototypeOf);

#undef OBJ_METHOD
}

