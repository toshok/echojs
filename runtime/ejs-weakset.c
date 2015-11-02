/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-weakset.h"
#include "ejs-set.h"
#include "ejs-array.h"
#include "ejs-gc.h"
#include "ejs-error.h"
#include "ejs-function.h"
#include "ejs-proxy.h"
#include "ejs-ops.h"
#include "ejs-symbol.h"


#define EJSVAL_IS_WEAKSET(v)     (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_WeakSet_specops))
#define EJSVAL_TO_WEAKSET(v)     ((EJSMap*)EJSVAL_TO_OBJECT(v))

static ejsval _ejs_WeakSetData_symbol EJSVAL_ALIGNMENT;

ejsval
_ejs_weakset_new ()
{
    EJSObject *set = _ejs_gc_new (EJSObject);
    _ejs_init_object (set, _ejs_WeakSet_prototype, &_ejs_WeakSet_specops);

    return OBJECT_TO_EJSVAL(set);
}

// ES6: 23.4.3.1
// WeakSet.prototype.add ( value )
ejsval
_ejs_WeakSet_prototype_add(ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval value = _ejs_undefined;
    if (argc > 0) value = args[0];

    // 1. Let S be the this value.
    ejsval S = _this;

    // 2. If Type(S) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "add called with non-object this.");

    // 3. If S does not have a [[WeakSetData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_WEAKSET(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "add called with non-WeakSet this.");

    // 4. If S’s [[WeakSetData]] internal slot is undefined, then throw a TypeError exception.

    // 6. If Type(value) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(value))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "add called with non-Object value.");

#if WEAK_COLLECTIONS_USE_INVERTED_REP
    ejsval iset = _ejs_object_getprop(value, _ejs_WeakSetData_symbol);
    if (EJSVAL_IS_NULL_OR_UNDEFINED(iset)) {
        iset = _ejs_set_new();
        _ejs_object_setprop(value, _ejs_WeakSetData_symbol, iset);
    }

    if (!EJSVAL_IS_SET(iset))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "[[WeakSetData]] internal error");

    _ejs_set_add (iset, S);
    return S;
#else
    // 7. Let entries be the List that is the value of S’s [[WeakSetData]] internal slot.
    //    a. If e is not empty and SameValue(e, value) is true, then
    //       1. Return S.
    // 8. Append value as the last element of entries.
    // 9. Return S.
#endif
}


// ES6: 23.4.3.3
// WeakSet.prototype.delete ( value )
ejsval
_ejs_WeakSet_prototype_delete(ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval value = _ejs_undefined;
    if (argc > 0) value = args[0];

    // 1. Let S be the this value.
    ejsval S = _this;

    // 2. If Type(S) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "delete called with non-object this.");

    // 3. If S does not have a [[WeakSetData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_WEAKSET(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "delete called with non-WeakSet this.");

    // 4. If S’s [[WeakSetData]] internal slot is undefined, then throw a TypeError exception.

    // 5. If Type(value) is not Object, then return false.
    if (!EJSVAL_IS_OBJECT(value))
        return _ejs_false;

    // 6. Let entries be the List that is the value of M’s [[WeakSetData]] internal slot.

#if WEAK_COLLECTIONS_USE_INVERTED_REP
    ejsval iset = _ejs_object_getprop(value, _ejs_WeakSetData_symbol);
    if (EJSVAL_IS_NULL_OR_UNDEFINED(iset))
        return _ejs_false;

    if (!EJSVAL_IS_SET(iset))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "[[WeakSetData]] internal error");

    return _ejs_set_delete (iset, S);
#else    
    // 7. Repeat for each e that is an element of entries,
    //    a. If e is not empty and SameValue(e, value) is true, then
    //       i. Replace the element of entries whose value is e with an element whose value is empty.
    //       ii. Return true.
    // 8. Return false.
    return _ejs_false;
#endif
}

// ES6: 23.4.3.4
// WeakSet.prototype.has ( value )
ejsval
_ejs_WeakSet_prototype_has(ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval value = _ejs_undefined;
    if (argc > 0) value = args[0];

    // 1. Let S be the this value.
    ejsval S = _this;

    // 2. If Type(S) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "has called with non-object this.");

    // 3. If M does not have a [[WeakSetData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_WEAKSET(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "has called with non-WeakSet this.");

    // 4. If S’s [[WeakSetData]] internal slot is undefined, then throw a TypeError exception.

    // 5. If Type(value) is not Object, then return false.
    if (!EJSVAL_IS_OBJECT(value))
        return _ejs_false;

    // 6. Let entries be the List that is the value of M’s [[WeakSetData]] internal slot.

#if WEAK_COLLECTIONS_USE_INVERTED_REP
    ejsval iset = _ejs_object_getprop(value, _ejs_WeakSetData_symbol);
    if (EJSVAL_IS_NULL_OR_UNDEFINED(iset))
        return _ejs_false;

    if (!EJSVAL_IS_SET(iset))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "[[WeakSetData]] internal error");

    return _ejs_set_has (iset, S);
#else    
    // 7. Repeat for each e that is an element of entries,
    //    a. If e is not empty and SameValue(e, value), then return true.
    // 8. Return false.
#endif
}

// ES6: 23.1.1.1
// WeakSet ()
static ejsval
_ejs_WeakSet_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let set be the this value.
    ejsval set = _this;

    // 2. If Type(set) is not Object then, throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(set))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "WeakSet constructor called with non-object this.");
    
    // 3. If set does not have a [[WeakSetData]] internal slot, then throw a TypeError exception.
    if (!EJSVAL_IS_WEAKSET(set))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "WeakSet constructor called with non-WeakSet this.");

    // 4. If set’s [[WeakSetData]] internal slot is not undefined, then throw a TypeError exception.

    // 5. If iterable is not present, let iterable be undefined. 
    ejsval iterable = _ejs_undefined;
    if (argc > 0)
        iterable = args[0];
    ejsval iter = _ejs_undefined;
    ejsval adder = _ejs_undefined;

    // 6. If iterable is either undefined or null, then let iter be undefined.
    // 7. Else, 
    if (!EJSVAL_IS_UNDEFINED(iterable) && !EJSVAL_IS_NULL(iterable)) {
        //    a. Let iter be the result of GetIterator(iterable). 
        //    b. ReturnIfAbrupt(iter). 
        iter = GetIterator (iterable, _ejs_undefined);
        //    c. Let adder be the result of Get(set, "add").
        //    d. ReturnIfAbrupt(adder). 
        adder = Get (set, _ejs_atom_add);
        //    e. If IsCallable(adder) is false, throw a TypeError Exception.
        if (!IsCallable(adder))
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.add is not a function");
    }
    // 8. If the value of sets’s [[WeakSetData]] internal slot is not undefined, then throw a TypeError exception. 
    // 9. Assert: set has not been reentrantly initialized. 
    // 10. Set set’s [[WeakSetData]] internal slot to a new empty List.

    // 11. If iter is undefined, then return set. 
    if (EJSVAL_IS_UNDEFINED(iter))
        return set;

    // 12. Repeat 
    for (;;) {
        //    a. Let next be the result of IteratorStep(iter).
        //    b. ReturnIfAbrupt(next).
        ejsval next = IteratorStep (iter);

        //    c. If next is false, then return set.
        if (!EJSVAL_TO_BOOLEAN(next))
            return set;

        //    d. Let nextValue be IteratorValue(next).
        //    e. ReturnIfAbrupt(nextValue).
        ejsval nextValue = IteratorValue (next);

        //    f. Let status be the result of calling the [[Call]] internal method of adder with set as thisArgument
        //       and a List whose sole element is nextValue as argumentsList.
        //    g. ReturnIfAbrupt(status).
        _ejs_invoke_closure (adder, set, 1, &nextValue);
    }

    return set;
}

static ejsval
_ejs_WeakSet_create (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let F be the this value. 
    ejsval F = _this;

    if (!IsConstructor(F)) 
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "'this' in WeakSet[Symbol.create] is not a constructor");

    EJSObject* F_ = EJSVAL_TO_OBJECT(F);

    // 2. Let obj be the result of calling OrdinaryCreateFromConstructor(F, "%WeakSetPrototype%", ([[WeakSetData]]) ). 
    ejsval proto = OP(F_,Get)(F, _ejs_atom_prototype, F);
    if (EJSVAL_IS_UNDEFINED(proto))
        proto = _ejs_WeakSet_prototype;

    EJSObject* obj = (EJSObject*)_ejs_gc_new (EJSWeakSet);
    _ejs_init_object (obj, proto, &_ejs_WeakSet_specops);
    return OBJECT_TO_EJSVAL(obj);
}

ejsval _ejs_WeakSet EJSVAL_ALIGNMENT;
ejsval _ejs_WeakSet_prototype EJSVAL_ALIGNMENT;

void
_ejs_weakset_init(ejsval global)
{
    _ejs_gc_add_root (&_ejs_WeakSetData_symbol);
    _ejs_WeakSetData_symbol = _ejs_symbol_new(_ejs_atom_WeakSetData);

    _ejs_WeakSet = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_WeakSet, (EJSClosureFunc)_ejs_WeakSet_impl);
    _ejs_object_setprop (global, _ejs_atom_WeakSet, _ejs_WeakSet);

    _ejs_gc_add_root (&_ejs_WeakSet_prototype);
    _ejs_WeakSet_prototype = _ejs_object_new (_ejs_Object_prototype, &_ejs_Object_specops);
    _ejs_object_setprop (_ejs_WeakSet,       _ejs_atom_prototype,  _ejs_WeakSet_prototype);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_WeakSet, x, _ejs_WeakSet_##x)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_WeakSet_prototype, x, _ejs_WeakSet_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)
#define PROTO_GETTER(x) EJS_INSTALL_ATOM_GETTER(_ejs_WeakSet_prototype, x, _ejs_WeakSet_prototype_get_##x)

    PROTO_METHOD(add);
    // XXX (ES6 23.4.3.2) WeakSet.prototype.constructor
    PROTO_METHOD(delete);
    PROTO_METHOD(has);

    _ejs_object_define_value_property (_ejs_WeakSet_prototype, _ejs_Symbol_toStringTag, _ejs_atom_WeakSet, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_WeakSet, create, _ejs_WeakSet_create, EJS_PROP_NOT_ENUMERABLE);

#undef OBJ_METHOD
#undef PROTO_METHOD
}

EJS_DEFINE_INHERIT_ALL_CLASS(WeakSet)
