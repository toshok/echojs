/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-set.h"
#include "ejs-array.h"
#include "ejs-gc.h"
#include "ejs-error.h"
#include "ejs-function.h"
#include "ejs-proxy.h"
#include "ejs-ops.h"
#include "ejs-symbol.h"

typedef EJSBool (*ComparatorFunc)(ejsval, ejsval);

ejsval
_ejs_set_new ()
{
    EJSSet *set = _ejs_gc_new (EJSSet);
    _ejs_init_object ((EJSObject*)set, _ejs_Set_prototype, &_ejs_Set_specops);

    return OBJECT_TO_EJSVAL(set);
}

// ES6: 23.1.3.1
// Map.prototype.clear ()
ejsval
_ejs_Set_prototype_clear (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let S be this value. 
    ejsval S = _this;

    // 2. If Type(S) is not Object, then throw a TypeError exception. 
    if (!EJSVAL_IS_OBJECT(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.clear called with non-object this.");

    // 3. If S does not have a [[SetData]] internal slot throw a TypeError exception. 
    if (!EJSVAL_IS_SET(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.clear called with non-Set this.");

    // 4. If S’s [[SetData]] internal slot is undefined, then throw a TypeError exception. 

    // 5. Let entries be the List that is the value of S’s [[SetData]] internal slot. 
    EJSSetValueEntry* entries = EJSVAL_TO_SET(S)->head_insert;
    // 6. Repeat for each e that is an element of entries, 
    for (EJSSetValueEntry* e = entries; e; e = e->next_insert) {
        //    a. Replace the element of entries whose value is e with an element whose value is empty. 
        e->value = MAGIC_TO_EJSVAL_IMPL(EJS_NO_ITER_VALUE);
    }
    // 7. Return undefined. 
    return _ejs_undefined;
}

ejsval
_ejs_set_delete(ejsval S, ejsval value)
{
    // our caller should have already validated and thrown appropriate TypeErrors
    EJS_ASSERT(EJSVAL_IS_SET(S));

    // 5. Let entries be the List that is the value of S’s [[SetData]] internal slot. 
    EJSSetValueEntry* entries = EJSVAL_TO_SET(S)->head_insert;
    // 6. Repeat for each e that is an element of entries, 
    for (EJSSetValueEntry* e = entries; e; e = e->next_insert) {
        //    a. If e is not empty and SameValueZero(e, value) is true, then 
        if (SameValueZero(e->value, value)) {
            // i. Replace the element of entries whose value is e with an element whose value is empty. 
            e->value = MAGIC_TO_EJSVAL_IMPL(EJS_NO_ITER_VALUE);
            // ii. Return true. 
            return _ejs_true;
        }
    }
    // 7. Return false. 
    return _ejs_false;
}

// 23.2.3.4 Set.prototype.delete ( value ) 
ejsval
_ejs_Set_prototype_delete (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // NOTE The value empty is used as a specification device to
    // indicate that an entry has been deleted. Actual implementations
    // may take other actions such as physically removing the entry
    // from internal data structures.

    ejsval value = _ejs_undefined;
    if (argc > 0) value = args[0];

    // 1. Let S be the this value. 
    ejsval S = _this;

    // 2. If Type(S) is not Object, then throw a TypeError exception. 
    if (!EJSVAL_IS_OBJECT(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.delete called with non-object this.");

    // 3. If S does not have a [[SetData]] internal slot throw a TypeError exception. 
    if (!EJSVAL_IS_SET(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.delete called with non-Set this.");

    // 4. If S’s [[SetData]] internal slot is undefined, then throw a TypeError exception. 

    return _ejs_set_delete(S, value);
}

// ES6: 23.2.3.5
// Set.prototype.entries ( )
ejsval
_ejs_Set_prototype_entries (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let S be the this value.
    ejsval S = _this;

    // 2. Return the result of calling the CreateSetIterator abstract operation with arguments S and "key+value".
    return _ejs_set_iterator_new (S, EJS_SET_ITER_KIND_KEYVALUE);
}


// ES6: 23.2.3.6 Set.prototype.forEach ( callbackfn , thisArg = undefined )
ejsval
_ejs_Set_prototype_forEach (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval callbackfn = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc > 0) callbackfn = args[0];
    if (argc > 1) thisArg = args[1];

    // 1. Let S be the this value. 
    ejsval S = _this;

    // 2. If Type(S) is not Object, then throw a TypeError exception. 
    if (!EJSVAL_IS_OBJECT(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.forEach called with non-object this.");

    // 3. If S does not have a [[SetData]] internal slot throw a TypeError exception. 
    if (!EJSVAL_IS_SET(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.forEach called with non-Set this.");

    // 4. If S’s [[SetData]] internal slot is undefined, then throw a TypeError exception. 

    // 5. If IsCallable(callbackfn) is false, throw a TypeError exception. 
    if (!EJSVAL_IS_CALLABLE(callbackfn))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.forEach callbackfn isn't a function.");

    // 6. If thisArg was supplied, let T be thisArg; else let T be undefined. 
    ejsval T = thisArg;

    EJSSet* set = EJSVAL_TO_SET(S);

    // 7. Let entries be the List that is the value of S’s [[SetData]] internal slot. 
    EJSSetValueEntry* entries = set->head_insert;

    // 8. Repeat for each e that is an element of entries, in original insertion order 
    for (EJSSetValueEntry *e = entries; e; e = e->next_insert) {
        //    a. If e is not empty, then 
        if (EJSVAL_IS_NO_ITER_VALUE_MAGIC(e->value))
            continue;

        //       i. Let funcResult be the result of calling the [[Call]] internal method of callbackfn with T as thisArgument and a List containing e, e, and S as argumentsList. 
        //       ii. ReturnIfAbrupt(funcResult). 
        ejsval callback_args[3];
        callback_args[0] = e->value;
        callback_args[1] = e->value;
        callback_args[2] = S;
        _ejs_invoke_closure (callbackfn, T, 3, callback_args);

    }

    // 9. Return undefined. 
    return _ejs_undefined;
}

ejsval
_ejs_set_has(ejsval S, ejsval value)
{
    // our caller should have already validated and thrown appropriate TypeErrors
    EJS_ASSERT(EJSVAL_IS_SET(S));


    EJSSet* _set = EJSVAL_TO_SET(S);

    // 5. Let entries be the List that is the value of S’s [[SetData]] internal slot. 
    EJSSetValueEntry* entries = _set->head_insert;

    // 6. Repeat for each e that is an element of entries, 
    for (EJSSetValueEntry* e = entries; e; e = e->next_insert) {
        // a. If e is not empty and SameValueZero(e, value) is true, then return true.
        if (SameValueZero (e->value, value))
            return _ejs_true;
    }
    // 7. Return false. 
    return _ejs_false;
}

// ES6: 23.2.3.7
// Set.prototype.has ( value )
ejsval
_ejs_Set_prototype_has (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval value = _ejs_undefined;
    if (argc > 0) value = args[0];

    // 1. Let S be the this value. 
    ejsval S = _this;

    // 2. If Type(S) is not Object, then throw a TypeError exception. 
    if (!EJSVAL_IS_OBJECT(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.has called with non-object this.");

    // 3. If S does not have a [[SetData]] internal slot throw a TypeError exception. 
    if (!EJSVAL_IS_SET(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.has called with non-Set this.");

    // 4. If S’s [[SetData]] internal slot is undefined, then throw a TypeError exception. 

    return _ejs_set_has(S, value);
}

ejsval
_ejs_set_add(ejsval S, ejsval value)
{
    // our caller should have already validated and thrown appropriate TypeErrors
    EJS_ASSERT(EJSVAL_IS_SET(S));

    // 4. If S’s [[SetData]] internal slot is undefined, then throw a TypeError exception. 
    EJSSet* _set = EJSVAL_TO_SET(S);

    // 5. Let entries be the List that is the value of S’s [[SetData]] internal slot. 
    EJSSetValueEntry* entries = _set->head_insert;

    EJSSetValueEntry* e;
    // 6. Repeat for each e that is an element of entries, 
    for (e = entries; e; e = e->next_insert) {
        //    a. If e is not empty and SameValueZero(e, value) is true, then 
        if (SameValueZero(e->value, value))
        //       i. Return S. 
            return S;
    }
    // 7. If value is −0, then let value be +0. 
    if (EJSVAL_IS_NUMBER(value) && EJSDOUBLE_IS_NEGZERO(EJSVAL_TO_NUMBER(value)))
        value = NUMBER_TO_EJSVAL(0);
    // 8. Append value as the last element of entries. 
    e = calloc (1, sizeof (EJSSetValueEntry));
    e->value = value;

    if (!_set->head_insert)
        _set->head_insert = e;

    if (_set->tail_insert) {
        _set->tail_insert->next_insert = e;
        _set->tail_insert = e;
    }
    else {
        _set->tail_insert = e;
    }

    // 9. Return S.
    return S;
}

// ES6: 23.2.3.1 Set.prototype.add ( value )
ejsval
_ejs_Set_prototype_add (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval value = _ejs_undefined;
    if (argc > 0) value = args[0];

    // 1. Let S be the this value. 
    ejsval S = _this;

    // 2. If Type(S) is not Object, then throw a TypeError exception. 
    if (!EJSVAL_IS_OBJECT(S)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.add called with non-object this.");
    }

    // 3. If S does not have a [[SetData]] internal slot throw a TypeError exception. 
    if (!EJSVAL_IS_SET(S))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.set called with non-Set this.");

    return _ejs_set_add(S, value);
}

// ES6: 23.2.3.9
// get Set.prototype.size
static ejsval
_ejs_Set_prototype_get_size (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let S be the this value.
    ejsval S = _this;

    // 2. If Type(S) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(S)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set size getter called with non-object this.");
    }

    // 3. If S does not have a [[SetData]] internal slot throw a TypeError exception.
    if (!EJSVAL_IS_SET(S)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set size getter called with non-Set this.");
    }

    EJSSet* _set = EJSVAL_TO_SET(S);

    // 4. If S’s [[SetData]] internal slot is undefined, then throw a TypeError exception.

    // 5. Let entries be the List that is the value of S’s [[SetData]] internal slot.
    EJSSetValueEntry* entries = _set->head_insert;

    // 6. Let count be 0.
    uint32_t count = 0;
    // 7. For each e that is an element of entries
    for (EJSSetValueEntry* e = entries; e; e = e->next_insert) {
        //   a. If e is not empty then
        if (!EJSVAL_IS_NO_ITER_VALUE_MAGIC(e->value))
            //      i. Set count to count+1.
            count ++;
    }
    // 8. Return count.
    return NUMBER_TO_EJSVAL(count);
}

// ES6: 23.2.3.10
// Set.prototype.values ( )
ejsval
_ejs_Set_prototype_values (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let S be the this value. 
    ejsval S = _this;

    // 2. Return the result of calling the CreateSetIterator abstract operation with argument S and "value".
    return _ejs_set_iterator_new (S, EJS_SET_ITER_KIND_VALUE);
}

// ES6: 23.2.1.1 Set ( [ iterable ] )
static ejsval
_ejs_Set_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let set be the this value. 
    ejsval set = _this;

    if (EJSVAL_IS_UNDEFINED(set)) {
        EJSObject* obj = (EJSObject*)_ejs_gc_new(EJSSet);
        _ejs_init_object (obj, _ejs_Set_prototype, &_ejs_Set_specops);
        set = OBJECT_TO_EJSVAL(obj);
    }

    // 2. If Type(set) is not Object then, throw a TypeError exception. 
    if (!EJSVAL_IS_OBJECT(set))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set constructor called with non-object this.");

    // 3. If set does not have a [[SetData]] internal slot, then throw a TypeError exception. 
    if (!EJSVAL_IS_SET(set))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set constructor called with non-Set this.");

    EJSSet* _set = EJSVAL_TO_SET(set);

    // 4. If set’s [[SetData]] internal slot is not undefined, then throw a TypeError exception.
    if (_set->head_insert)
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set constructor called with an already initialized Set");

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
        if (!EJSVAL_IS_CALLABLE(adder))
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Set.prototype.add is not a function");
    }
    // 8. If the value of sets’s [[SetData]] internal slot is not undefined, then throw a TypeError exception. 
    // 9. Assert: set has not been reentrantly initialized. 
    // 10. Set set’s [[SetData]] internal slot to a new empty List.

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
_ejs_Set_get_species (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    return _ejs_Set;
}

// ECMA262: 23.2.2.2
// Set[ @@create ] ( ) 
static ejsval
_ejs_Set_create (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let F be the this value. 
    ejsval F = _this;

    if (!EJSVAL_IS_CONSTRUCTOR(F)) 
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "'this' in Set[Symbol.create] is not a constructor");

    EJSObject* F_ = EJSVAL_TO_OBJECT(F);

    // 2. Let obj be the result of calling OrdinaryCreateFromConstructor(F, "%SetPrototype%", ([[SetData]]) ). 
    ejsval proto = OP(F_,Get)(F, _ejs_atom_prototype, F);
    if (EJSVAL_IS_UNDEFINED(proto))
        proto = _ejs_Set_prototype;

    EJSObject* obj = (EJSObject*)_ejs_gc_new (EJSSet);
    _ejs_init_object (obj, proto, &_ejs_Set_specops);
    return OBJECT_TO_EJSVAL(obj);
}

ejsval _ejs_Set EJSVAL_ALIGNMENT;
ejsval _ejs_Set_prototype EJSVAL_ALIGNMENT;

ejsval
_ejs_set_iterator_new (ejsval set, EJSSetIteratorKind kind)
{
    /* 1. If Type(set) is not Object, throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(set))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "set is not a Object");

    /* 2. If set does not have a [[SetData]] internal slot throw a TypeError exception. */
    if (!EJSVAL_IS_SET(set))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "value is not a Set");

    /* 3. If set’s [[SetData]] internal slot is undefined, then throw a TypeError exception. */

    /* 4. Let iterator be the result of ObjectCreate(%SetIteratorPrototype%,
     * ([[IteratedSet]], [[SetNextIndex]], [[SetIterationKind]])). */
    EJSSetIterator *iter = _ejs_gc_new (EJSSetIterator);
    _ejs_init_object ((EJSObject*) iter, _ejs_SetIterator_prototype, &_ejs_SetIterator_specops);

    /* 5. Set iterator’s [[IteratedSet]] internal slot to set. */
    iter->iterated = set;

    /* 6. Set iterator’s [[SetNextIndex]] internal slot to 0. */
    iter->next_index = 0;

    /* 7. Set iterator’s [[SetIterationKind]] internal slot to kind. */
    iter->kind = kind;

    /* 8. Return iterator */
    return OBJECT_TO_EJSVAL(iter);
}

ejsval _ejs_SetIterator EJSVAL_ALIGNMENT;
ejsval _ejs_SetIterator_prototype EJSVAL_ALIGNMENT;

static ejsval
_ejs_SetIterator_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    return _this;
}

static ejsval
_ejs_SetIterator_prototype_next (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    /* 1. Let O be the this value. */
    ejsval O = _this;

    /* 2. If Type(O) is not Object, throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, ".next called on non-object");

    /* 3. If O does not have all of the internal slots of a Set Iterator Instance (23.2.5.3),
     * throw a TypeError exception. */
    if (!EJSVAL_IS_SETITERATOR(O))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, ".next called on non-SetIterator instance");

    EJSSetIterator *OObj = (EJSSetIterator*)EJSVAL_TO_OBJECT(O);

    /* 4. Let s be the value of the [[IteratedSet]] internal slot of O. */
    ejsval s = OObj->iterated;

    /* 5. Let index be the value of the [[SetNextIndex]] internal slot of O. */
    uint32_t index = OObj->next_index;

    /* 6. Let itemKind be the value of the [[SetIterationKind]] internal slot of O. */
    EJSSetIteratorKind itemKind = OObj->kind;

    /* 7. If s is undefined, then return CreateIterResultObject(undefined, true). */
    if (EJSVAL_IS_UNDEFINED(s))
        return _ejs_create_iter_result (_ejs_undefined, _ejs_true);

    /* 8. Assert: s has a [[SetData]] internal slot and s has been initialized so the value of
     * [[SetData]] is not undefined. */

    /* 9. Let entries be the List that is the value of the [[SetData]] internal slot of s. */
    EJSSetValueEntry *entries = EJSVAL_TO_SET(s)->head_insert;

    /* 10. Repeat while index is less than the total number of elements of entries. The number of elements must
     * be redetermined each time this method is evaluated. */
    uint32_t i = 0;
    for (EJSSetValueEntry *entry = entries; entry; entry = entry->next_insert) {

        /* Ignore this item if is marked as empty */
        if (EJSVAL_IS_NO_ITER_VALUE_MAGIC(entry->value))
            continue;

        /* Ignore this item if we haven't reached the initial needed point/index */
        if (index > i++)
            continue;

        /* a. Let e be entries[index]. */
        ejsval e = entry->value;

        /* b. Set index to index+1; */
        index = index + 1;

        /* c. Set the [[SetNextIndex]] internal slot of O to index. */
        OObj->next_index = index;

        /* d. If e is not empty, then */
        /*  (see EJSVAL_IS_NO_ITER_VALUE_MAGIC check at the beginning of the loop */

        /*      i. If itemKind is "key+value" then, */
        if (itemKind == EJS_SET_ITER_KIND_KEYVALUE) {
            /* 1. Let result be the result of performing ArrayCreate(2). */
            /* 2. Assert: result is a new, well-formed Array object so the following operations will never fail. */
            ejsval result = _ejs_array_new (2, EJS_FALSE);

            /* 3. Call CreateDataProperty(result, "0", e) . */
            _ejs_object_setprop (result, NUMBER_TO_EJSVAL(0), e);

            /* 4. Call CreateDataProperty(result, "1", e) . */
            _ejs_object_setprop (result, NUMBER_TO_EJSVAL(1), e);

            return _ejs_create_iter_result (result, _ejs_false);
        }

        /*      ii. Return CreateIterResultObject(e, false). */
        return _ejs_create_iter_result (e, _ejs_false);
    }

    /* 11. Set the [[IteratedSet]] internal slot of O to undefined. */
    OObj->iterated = _ejs_undefined;

    /* 12. Return CreateIterResultObject(undefined, true). */
    return _ejs_create_iter_result (_ejs_undefined, _ejs_true);
}

void
_ejs_set_init(ejsval global)
{
    _ejs_Set = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Set, (EJSClosureFunc)_ejs_Set_impl);
    _ejs_object_setprop (global, _ejs_atom_Set, _ejs_Set);

    _ejs_gc_add_root (&_ejs_Set_prototype);
    _ejs_Set_prototype = _ejs_set_new ();
    _ejs_object_setprop (_ejs_Set,       _ejs_atom_prototype,  _ejs_Set_prototype);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Set, x, _ejs_Set_##x)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_Set_prototype, x, _ejs_Set_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)
#define PROTO_GETTER(x) EJS_INSTALL_ATOM_GETTER(_ejs_Set_prototype, x, _ejs_Set_prototype_get_##x)

    PROTO_METHOD(add);
    PROTO_METHOD(clear);
    PROTO_METHOD(delete);
    PROTO_METHOD(entries);
    PROTO_METHOD(forEach);
    PROTO_METHOD(has);
    PROTO_GETTER(size);

    // expand PROTO_METHOD(values) here so that we can install the function for both keys and @@iterator below
    ejsval _values = _ejs_function_new_native (_ejs_null, _ejs_atom_values, (EJSClosureFunc)_ejs_Set_prototype_values);
    _ejs_object_define_value_property (_ejs_Set_prototype, _ejs_atom_values, _values, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_FLAGS_WRITABLE | EJS_PROP_CONFIGURABLE);
    _ejs_object_define_value_property (_ejs_Set_prototype, _ejs_atom_keys, _values, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);

    _ejs_object_define_value_property (_ejs_Set_prototype, _ejs_Symbol_iterator, _values, EJS_PROP_NOT_ENUMERABLE);
    _ejs_object_define_value_property (_ejs_Set_prototype, _ejs_Symbol_toStringTag, _ejs_atom_Set, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_Set, create, _ejs_Set_create, EJS_PROP_NOT_ENUMERABLE);

    EJS_INSTALL_SYMBOL_GETTER(_ejs_Set, species, _ejs_Set_get_species);

#undef OBJ_METHOD
#undef PROTO_METHOD

    _ejs_SetIterator = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Set, (EJSClosureFunc)_ejs_SetIterator_impl);

    _ejs_gc_add_root (&_ejs_SetIterator_prototype);
    _ejs_SetIterator_prototype = _ejs_set_iterator_new(_ejs_Set_prototype, EJS_SET_ITER_KIND_VALUE);
    EJSVAL_TO_OBJECT(_ejs_SetIterator_prototype)->proto = _ejs_Object_prototype;
    _ejs_object_define_value_property (_ejs_SetIterator, _ejs_atom_prototype, _ejs_SetIterator_prototype,
                                        EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property (_ejs_SetIterator_prototype, _ejs_atom_constructor, _ejs_SetIterator,
                                        EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

#define PROTO_ITER_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_SetIterator_prototype, x, _ejs_SetIterator_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)
    PROTO_ITER_METHOD(next);
#undef PROTO_ITER_METHOD

}

static EJSObject*
_ejs_set_specop_allocate ()
{
    return (EJSObject*)_ejs_gc_new (EJSSet);
}

static void
_ejs_set_specop_finalize (EJSObject* obj)
{
    EJSSet* set = (EJSSet*)obj;

    EJSSetValueEntry* s = set->head_insert;
    while (s) {
        EJSSetValueEntry* next = s->next_insert;
        free (s);
        s = next;
    }

    _ejs_Object_specops.Finalize (obj);
}

static void
_ejs_set_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSSet* set = (EJSSet*)obj;

    for (EJSSetValueEntry *s = set->head_insert; s; s = s->next_insert)
        scan_func (s->value);

    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(Set,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT, // [[IsExtensible]]
                 OP_INHERIT, // [[PreventExtensions]]
                 OP_INHERIT, // [[GetOwnProperty]]
                 OP_INHERIT, // [[DefineOwnProperty]]
                 OP_INHERIT, // [[HasProperty]]
                 OP_INHERIT, // [[Get]]
                 OP_INHERIT, // [[Set]]
                 OP_INHERIT, // [[Delete]]
                 OP_INHERIT, // [[Enumerate]]
                 OP_INHERIT, // [[OwnPropertyKeys]]
                 _ejs_set_specop_allocate,
                 _ejs_set_specop_finalize,
                 _ejs_set_specop_scan
                 )

static void
_ejs_set_iterator_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSSetIterator* iter = (EJSSetIterator*)obj;
    scan_func(iter->iterated);
    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(SetIterator,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT, // [[IsExtensible]]
                 OP_INHERIT, // [[PreventExtensions]]
                 OP_INHERIT, // [[GetOwnProperty]]
                 OP_INHERIT, // [[DefineOwnProperty]]
                 OP_INHERIT, // [[HasProperty]]
                 OP_INHERIT, // [[Get]]
                 OP_INHERIT, // [[Set]]
                 OP_INHERIT, // [[Delete]]
                 OP_INHERIT, // [[Enumerate]]
                 OP_INHERIT, // [[OwnPropertyKeys]]
                 OP_INHERIT, // allocate.  shouldn't ever be used
                 OP_INHERIT, // finalize.  also shouldn't ever be used
                 _ejs_set_iterator_specop_scan
                 )

