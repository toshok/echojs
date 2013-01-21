/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <assert.h>
#include <string.h>
#include <math.h>

#include "ejs.h"
#include "ejs-ops.h"
#include "ejs-array.h"
#include "ejs-function.h"

static ejsval  _ejs_array_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr);
static EJSPropertyDesc* _ejs_array_specop_get_own_property (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_array_specop_get_property (ejsval obj, ejsval propertyName);
static void    _ejs_array_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag);
static EJSBool _ejs_array_specop_can_put (ejsval obj, ejsval propertyName);
static EJSBool _ejs_array_specop_has_property (ejsval obj, ejsval propertyName);
static EJSBool _ejs_array_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag);
static ejsval  _ejs_array_specop_default_value (ejsval obj, const char *hint);
static EJSBool _ejs_array_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag);
static EJSObject* _ejs_array_specop_allocate ();
static void    _ejs_array_specop_finalize (EJSObject* obj);
static void    _ejs_array_specop_scan (EJSObject* obj, EJSValueFunc scan_func);

EJSSpecOps _ejs_array_specops = {
    "Array",
    _ejs_array_specop_get,
    _ejs_array_specop_get_own_property,
    _ejs_array_specop_get_property,
    _ejs_array_specop_put,
    _ejs_array_specop_can_put,
    _ejs_array_specop_has_property,
    _ejs_array_specop_delete,
    _ejs_array_specop_default_value,
    _ejs_array_specop_define_own_property,

    _ejs_array_specop_allocate,
    _ejs_array_specop_finalize,
    _ejs_array_specop_scan
};

EJSSpecOps _ejs_sparsearray_specops;

#define EJSVAL_IS_DENSE_ARRAY(v) (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_array_specops))
#define EJSVAL_IS_SPARSE_ARRAY(v) (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_sparsearray_specops))
#define EJS_ARRAY_IS_SPARSE(arrobj) ((arrobj)->ops == &_ejs_sparsearray_specops))

#define _EJS_ARRAY_LEN(arrobj)      (((EJSArray*)arrobj)->array_length)
#define _EJS_ARRAY_ELEMENTS(arrobj) (((EJSArray*)arrobj)->elements)

ejsval
_ejs_array_new (int numElements)
{
    EJSArray* rv = _ejs_gc_new (EJSArray);

    _ejs_init_object ((EJSObject*)rv, _ejs_Array_proto, &_ejs_array_specops);

    rv->array_length = numElements;
    rv->array_alloc = numElements + 500;
    rv->elements = (ejsval*)calloc(rv->array_alloc, sizeof (ejsval));
    return OBJECT_TO_EJSVAL((EJSObject*)rv);
}

void
_ejs_array_foreach_element (EJSArray* arr, EJSValueFunc foreach_func)
{
    for (int i = 0; i < arr->array_length; i ++)
        foreach_func (arr->elements[i]);
}

uint32_t
_ejs_array_push_dense(ejsval array, int argc, ejsval *args)
{
    EJSArray *arr = (EJSArray*)EJSVAL_TO_OBJECT(array);
    for (int i = 0; i < argc; i ++)
        EJSARRAY_ELEMENTS(arr)[EJSARRAY_LEN(arr)++] = args[i];
    return EJSARRAY_LEN(arr);
}

ejsval
_ejs_array_pop_dense(ejsval array)
{
    if (EJS_ARRAY_LEN(array) == 0)
        return _ejs_undefined;
    return EJS_ARRAY_ELEMENTS(array)[--EJS_ARRAY_LEN(array)];
}

ejsval _ejs_Array_proto;
ejsval _ejs_Array;

static ejsval
_ejs_Array_impl (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    if (EJSVAL_IS_UNDEFINED(_this)) {
        // called as a function

        if (argc == 0) {
            return _ejs_array_new(10);
        }
        else if (argc == 1 && EJSVAL_IS_NUMBER(args[0])) {
            return _ejs_array_new((int)EJSVAL_TO_NUMBER(args[0]));
        }
        else {
            ejsval rv = _ejs_array_new(argc);
            int i;

            for (i = 0; i < argc; i ++) {
                _ejs_object_setprop (rv, NUMBER_TO_EJSVAL(i), args[i]);
            }

            return rv;
        }
    }
    else {
        int alloc = 25;

        if (argc == 1 && EJSVAL_IS_NUMBER(args[0])) {
            alloc = (int)EJSVAL_TO_NUMBER(args[0]);
        }

        EJSArray* arr = (EJSArray*)EJSVAL_TO_OBJECT(_this);
        arr->array_length = 0;
        arr->array_alloc = alloc + 500;
        arr->elements = (ejsval*)calloc(arr->array_alloc, sizeof (ejsval));

        // called as a constructor
        return _this;
    }
}

static ejsval
_ejs_Array_prototype_shift (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    // EJS fast path for dense arrays
    if (EJSVAL_IS_DENSE_ARRAY(_this)) {
        int len = EJS_ARRAY_LEN(_this);
        if (len == 0) {
            return _ejs_undefined;
        }
        ejsval first = EJS_ARRAY_ELEMENTS(_this)[0];
        memmove (EJS_ARRAY_ELEMENTS(_this), EJS_ARRAY_ELEMENTS(_this) + 1, sizeof(ejsval) * len-1);
        EJS_ARRAY_LEN(_this) --;
        return first;
    }

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    ejsval O = ToObject(_this);


    EJS_NOT_IMPLEMENTED();
#if notyet
    // 2. Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    ejsval lenVal = OP(O,get) (O, _ejs_atom_length, EJS_FALSE);

    // 3. Let len be ToUint32(lenVal).
    int len = ToUint32(lenVal);

    // 4. If len is zero, then
    if (len == 0) {
        //   a. Call the [[Put]] internal method of O with arguments "length", 0, and true.
        OP(O,put) (O, _ejs_atom_length, 0, EJS_TRUE);
        //   b. Return undefined.
        return _ejs_undefined;
    }

    // 5. Let first be the result of calling the [[Get]] internal method of O with argument "0".
    ejsval first = OP(O,get) (O, _ejs_string_new_utf8 ("0"));

    // 6. Let k be 1.
    int k = 1;
    // 7. Repeat, while k < len
    while (k < len) {
        //   a. Let from be ToString(k).
        //   b. Let to be ToString(k–1).
        //   c. Let fromPresent be the result of calling the [[HasProperty]] internal method of O with argument from.
        //   d. If fromPresent is true, then
        //     i. Let fromVal be the result of calling the [[Get]] internal method of O with argument from.
        //     ii. Call the [[Put]] internal method of O with arguments to, fromVal, and true.
        //   e. Else, fromPresent is false
        //     i. Call the [[Delete]] internal method of O with arguments to and true.
        //   f. Increase k by 1.
        k++;
    }
    // 8. Call the [[Delete]] internal method of O with arguments ToString(len–1) and true.
    // 9. Call the [[Put]] internal method of O with arguments "length", (len–1) , and true.
    // 10. Return first.
#endif
}

static ejsval
_ejs_Array_prototype_unshift (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    // EJS fast path for arrays
    if (EJSVAL_IS_DENSE_ARRAY(_this)) {
        int len = EJS_ARRAY_LEN(_this);
        memmove (EJS_ARRAY_ELEMENTS(_this) + argc, EJS_ARRAY_ELEMENTS(_this), sizeof(ejsval) * len);
        memmove (EJS_ARRAY_ELEMENTS(_this), args, sizeof(ejsval) * argc);
        EJS_ARRAY_LEN(_this) += argc;
        return NUMBER_TO_EJSVAL(len + argc);
    }

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    ejsval O = ToObject(_this);

    EJS_NOT_IMPLEMENTED();

    // 2. Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    // 3. Let len be ToUint32(lenVal).
    // 4. Let argCount be the number of actual arguments.
    // 5. Let k be len.
    // 6. Repeat, while k > 0,
    //   a. Let from be ToString(k–1).
    //   b. Let to be ToString(k+argCount –1).
    //   c. Let fromPresent be the result of calling the [[HasProperty]] internal method of O with argument from.
    //   d. If fromPresent is true, then
    //      i. Let fromValue be the result of calling the [[Get]] internal method of O with argument from.
    //      ii. Call the [[Put]] internal method of O with arguments to, fromValue, and true.
    //   e. Else, fromPresent is false
    //      i. Call the [[Delete]] internal method of O with arguments to, and true.
    //   f. Decrease k by 1.
    // 7. Let j be 0.
    // 8. Let items be an internal List whose elements are, in left to right order, the arguments that were passed to this function invocation.
    // 9. Repeat, while items is not empty
    //   a. Remove the first element from items and let E be the value of that element.
    //   b. Call the [[Put]] internal method of O with arguments ToString(j), E, and true.
    //   c. Increase j by 1.
    // 10. Call the [[Put]] internal method of O with arguments "length", len+argCount, and true.
    // 11. Return len+argCount.
}

// ECMA262: 15.4.4.7
static ejsval
_ejs_Array_prototype_push (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    if (EJSVAL_IS_DENSE_ARRAY(_this)) {
        return NUMBER_TO_EJSVAL (_ejs_array_push_dense (_this, argc, args));
    }

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    ejsval O = ToObject(_this);

    EJS_NOT_IMPLEMENTED();

    // 2. Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    ejsval lenVal = OP(EJSVAL_TO_OBJECT(O),get)(O, _ejs_atom_length, EJS_FALSE);
    // 3. Let n be ToUint32(lenVal).
    uint32_t n = ToUint32(lenVal);

    // 4. Let items be an internal List whose elements are, in left to right order, the arguments that were passed to this 
    //    function invocation.
    // 5. Repeat, while items is not empty
    //    a. Remove the first element from items and let E be the value of the element.
    //    b. Call the [[Put]] internal method of O with arguments ToString(n), E, and true.
    //    c. Increase n by 1.
    // 6. Call the [[Put]] internal method of O with arguments "length", n, and true.
    // 7. Return n.
}

// ECMA262: 15.4.4.6
static ejsval
_ejs_Array_prototype_pop (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    if (EJSVAL_IS_DENSE_ARRAY(_this)) {
        return _ejs_array_pop_dense (_this);
    }

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    ejsval O = ToObject(_this);

    // 2. Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    ejsval lenVal = OP(EJSVAL_TO_OBJECT(O),get)(O, _ejs_atom_length, EJS_FALSE);

    // 3. Let len be ToUint32(lenVal).
    uint32_t len = ToUint32(lenVal);

    // 4. If len is zero, 
    if (len == 0) {
        //    a. Call the [[Put]] internal method of O with arguments "length", 0, and true.

        // EJS: why is a done above? to overwrite a accessor property?

        //    b. Return undefined.
        return _ejs_undefined;
    }
    // 5. Else, len > 0
    else {
        EJS_NOT_IMPLEMENTED();
        //    a. Let indx be ToString(len–1).
        //    b. Let element be the result of calling the [[Get]] internal method of O with argument indx.
        //    c. Call the [[Delete]] internal method of O with arguments indx and true.
        //    d. Call the [[Put]] internal method of O with arguments "length", indx, and true.
        //    e. Return element.
    }
}

static ejsval
_ejs_Array_prototype_concat (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    int numElements;

    // hacky fast path for everything being an array
    numElements = EJS_ARRAY_LEN(_this);
    int i;
    for (i = 0; i < argc; i ++) {
        numElements += EJS_ARRAY_LEN(args[i]);
    }

    ejsval rv = _ejs_array_new(numElements);
    numElements = 0;
    memmove (EJS_ARRAY_ELEMENTS(rv) + numElements, EJS_ARRAY_ELEMENTS(_this), sizeof(ejsval) * EJS_ARRAY_LEN(_this));
    numElements += EJS_ARRAY_LEN(_this);
    for (i = 0; i < argc; i ++) {
        memmove (EJS_ARRAY_ELEMENTS(rv) + numElements, EJS_ARRAY_ELEMENTS(args[i]), sizeof(ejsval) * EJS_ARRAY_LEN(args[i]));
        numElements += EJS_ARRAY_LEN(args[i]);
    }
    EJS_ARRAY_LEN(rv) = numElements;

    return rv;
}

static ejsval
_ejs_Array_prototype_slice (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    int len = EJS_ARRAY_LEN(_this);
    int begin = argc > 0 ? (int)EJSVAL_TO_NUMBER(args[0]) : 0;
    int end = argc > 1 ? (int)EJSVAL_TO_NUMBER(args[1]) : len;

    begin = MIN(begin, len);
    end = MIN(end, len);

    ejsval rv = _ejs_array_new(end-begin);
    int i, rv_i;

    rv_i = 0;
    for (i = begin; i < end; i ++, rv_i++) {
        _ejs_object_setprop (rv, NUMBER_TO_EJSVAL (rv_i), EJS_ARRAY_ELEMENTS(_this)[i]);
    }

    return rv;
}

static ejsval
_ejs_Array_prototype_join (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    if (EJS_ARRAY_LEN(_this) == 0)
        return _ejs_string_new_utf8 ("");

    const char* separator;
    int separator_len;

    if (argc > 0) {
        ejsval sepToString = ToString(args[0]);
        separator_len = EJSVAL_TO_STRLEN(sepToString);
        separator = EJSVAL_TO_FLAT_STRING(sepToString);
    }
    else {
        separator = ",";
        separator_len = 1;
    }
  
    char* result;
    int result_len = 0;

    ejsval* strings = (ejsval*)malloc (sizeof (ejsval) * EJS_ARRAY_LEN(_this));
    int i;

    for (i = 0; i < EJS_ARRAY_LEN(_this); i ++) {
        strings[i] = ToString(EJS_ARRAY_ELEMENTS(_this)[i]);
        result_len += EJSVAL_TO_STRLEN(strings[i]);
    }

    result_len += separator_len * (EJS_ARRAY_LEN(_this)-1) + 1/* \0 terminator */;

    result = (char*)malloc (result_len);
    int offset = 0;
    for (i = 0; i < EJS_ARRAY_LEN(_this); i ++) {
        int slen = EJSVAL_TO_STRLEN(strings[i]);
        memmove (result + offset, EJSVAL_TO_FLAT_STRING(strings[i]), slen);
        offset += slen;
        if (i < EJS_ARRAY_LEN(_this)-1) {
            memmove (result + offset, separator, separator_len);
            offset += separator_len;
        }
    }
    result[result_len-1] = 0;

    ejsval rv = _ejs_string_new_utf8(result);

    free (result);
    free (strings);

    return rv;
}

static ejsval
_ejs_Array_prototype_forEach (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    if (argc < 1)
        EJS_NOT_IMPLEMENTED();

    ejsval fun = args[0];

    int i;
    for (i = 0; i < EJS_ARRAY_LEN(_this); i ++) {
        _ejs_invoke_closure_2 (fun, _ejs_null, 2, EJS_ARRAY_ELEMENTS(_this)[i], NUMBER_TO_EJSVAL(i));
    }
    return _ejs_undefined;
}

static inline int
max(int a, int b)
{
    if (a > b) return a; else return b;
}

static inline int
min(int a, int b)
{
    if (a > b) return b; else return a;
}

// ECMA262: 15.4.4.12
static ejsval
_ejs_Array_prototype_splice (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    // start, deleteCount [ , item1 [ , item2 [ , … ] ] ]

    ejsval start = _ejs_undefined;
    ejsval deleteCount = _ejs_undefined;

    if (argc > 0) start = args[0];
    if (argc > 1) deleteCount = args[1];

    /* 1. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);
    /* 2. Let A be a new array created as if by the expression new Array() where Array is the standard built-in 
          constructor with that name. */
    ejsval A = _ejs_array_new(0);
    /* 3. Let lenVal be the result of calling the [[Get]] internal method of O with argument "length". */
    ejsval lenVal = _ejs_object_getprop (O, _ejs_atom_length);
    /* 4. Let len be ToUint32(lenVal). */
    uint32_t len = ToUint32(lenVal);
    /* 5. Let relativeStart be ToInteger(start). */
    int32_t relativeStart = ToInteger(start);
    /* 6. If relativeStart is negative, let actualStart be max((len + relativeStart),0); else let actualStart be 
          min(relativeStart, len). */
    int32_t actualStart = relativeStart < 0 ? max((len + relativeStart),0) : min(relativeStart, len);

    if (EJSVAL_IS_UNDEFINED(deleteCount))
        deleteCount = NUMBER_TO_EJSVAL (len - actualStart);
    /* 7. Let actualDeleteCount be min(max(ToInteger(deleteCount),0), len - actualStart). */
    int32_t actualDeleteCount = min(max(ToInteger(deleteCount),0), len - actualStart);
    /* 8. Let k be 0. */
    int k = 0;
    /* 9. Repeat, while k < actualDeleteCount */
    while (k < actualDeleteCount) {
        /*    a. Let from be ToString(actualStart+k). */
        ejsval from = NUMBER_TO_EJSVAL(actualStart+k);
        /*    b. Let fromPresent be the result of calling the [[HasProperty]] internal method of O with argument 
              from. */
        EJSBool fromPresent = OP(EJSVAL_TO_OBJECT(O), has_property)(O, from);
        /*    c. If fromPresent is true, then */
        if (fromPresent) {
            /*       i. Let fromValue be the result of calling the [[Get]] internal method of O with argument from. */
            ejsval fromValue = _ejs_object_getprop (O, from);
            /*       ii. Call the [[DefineOwnProperty]] internal method of A with arguments ToString(k), Property
                         Descriptor {[[Value]]: fromValue, [[Writable]]: true, [[Enumerable]]: true, 
                         [[Configurable]]: true}, and false. */
#if false
            EJSPropertyDesc desc = { .value = fromValue, .writable = EJS_TRUE, .enumerable = EJS_TRUE, .configurable = EJS_TRUE };
            OP(EJSVAL_TO_OBJECT(A), define_own_property)(A, NumberToString(k), &desc, EJS_FALSE);
#else
            _ejs_object_setprop (A, NUMBER_TO_EJSVAL(k), fromValue);
#endif
        }
        /*    d. Increment k by 1. */
        ++k;
    }
    /* 10. Let items be an internal List whose elements are, in left to right order, the portion of the actual argument list
           starting with item1. The list will be empty if no such items are present. */
    ejsval *items = &args[2];
    /* 11. Let itemCount be the number of elements in items. */
    int itemCount = argc > 2 ? argc - 2 : 0;
    /* 12. If itemCount < actualDeleteCount, then */
    if (itemCount < actualDeleteCount) {
        /*     a. Let k be actualStart. */
        k = actualStart;
        /*     b. Repeat, while k < (len – actualDeleteCount) */
        while (k < (len - actualDeleteCount)) {
            /*        i. Let from be ToString(k+actualDeleteCount). */
            ejsval from = NUMBER_TO_EJSVAL(k+actualDeleteCount);
            /*        ii. Let to be ToString(k+itemCount). */
            ejsval to = NUMBER_TO_EJSVAL(k+itemCount);
            /*        iii. Let fromPresent be the result of calling the [[HasProperty]] internal method of O with 
                           argument from. */
            EJSBool fromPresent = OP(EJSVAL_TO_OBJECT(O), has_property)(O, from);
            /*        iv. If fromPresent is true, then */
            if (fromPresent) {
                /*            1. Let fromValue be the result of calling the [[Get]] internal method of O with 
                                 argument from. */
                ejsval fromValue = _ejs_object_getprop (O, from);
                /*            2. Call the [[Put]] internal method of O with arguments to, fromValue, and true. */
                _ejs_object_setprop (O, to, fromValue);
            }
            /*        v. Else, fromPresent is false */
            else {
                /*            1. Call the [[Delete]] internal method of O with arguments to and true. */
                OP(EJSVAL_TO_OBJECT(O), _delete)(O, to, EJS_TRUE);
            }
            /*        vi. Increase k by 1. */
            ++k;
        }
        /*     c. Let k be len. */
        k = len;
        /*     d. Repeat, while k > (len - actualDeleteCount + itemCount)  */
        while (k > (len - actualDeleteCount + itemCount)) {
            /*        i. Call the [[Delete]] internal method of O with arguments ToString(k–1) and true. */
            OP(EJSVAL_TO_OBJECT(O), _delete)(O, NUMBER_TO_EJSVAL(k-1), EJS_TRUE);
            /*        ii. Decrease k by 1. */
            --k;
        }
    }
    /* 13. Else if itemCount > actualDeleteCount, then */
    else if (itemCount > actualDeleteCount) {
        /*     a. Let k be (len – actualDeleteCount). */
        k = (len - actualDeleteCount);
        /*     b. Repeat, while k > actualStart */
        while (k > actualStart) {
            /*        i. Let from be ToString(k + actualDeleteCount - 1). */
            ejsval from = NumberToString (k + actualDeleteCount - 1);
            /*        ii. Let to be ToString(k + itemCount - 1) */
            ejsval to = NumberToString(k + itemCount - 1);
            /*        iii. Let fromPresent be the result of calling the [[HasProperty]] internal method of O with 
                           argument from. */
            EJSBool fromPresent = OP(EJSVAL_TO_OBJECT(O), has_property)(O, from);
            /*        iv. If fromPresent is true, then */
            if (fromPresent) {
                /*            1. Let fromValue be the result of calling the [[Get]] internal method of O with 
                                 argument from. */
                ejsval fromValue = _ejs_object_getprop (O, from);
                /*            2. Call the [[Put]] internal method of O with arguments to, fromValue, and true. */
                _ejs_object_setprop (O, to, fromValue);
            }
            /*        v. Else, fromPresent is false */
            else {
                /*           1. Call the [[Delete]] internal method of O with argument to and true */
                OP(EJSVAL_TO_OBJECT(O), _delete)(O, to, EJS_TRUE);
            }
            /*        vi. Decrease k by 1. */
            --k;
        }
    }
    /* 14. Let k be actualStart. */
    k = actualStart;
    int item_i = 0;
    /* 15. Repeat, while items is not empty */
    while (item_i < itemCount) {
        /*     a. Remove the first element from items and let E be the value of that element. */
        ejsval E = items[item_i++];
        /*     b. Call the [[Put]] internal method of O with arguments ToString(k), E, and true. */
        _ejs_object_setprop (O, NUMBER_TO_EJSVAL(k), E);
        /*     c. Increase k by 1. */
        ++k;
    }
    /* 16. Call the [[Put]] internal method of O with arguments "length", (len - actualDeleteCount + itemCount),  
           and true. */
    _ejs_object_setprop (O, _ejs_atom_length, NUMBER_TO_EJSVAL(len - actualDeleteCount + itemCount));
    /* 17. Return A. */
    return A;
}

static ejsval
_ejs_Array_prototype_indexOf (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    if (argc != 1)
        return NUMBER_TO_EJSVAL (-1);

    int i;
    int rv = -1;
    ejsval needle = args[0];
    if (EJSVAL_IS_NULL(needle)) {
        for (i = 0; i < EJS_ARRAY_LEN(_this); i ++) {
            if (EJSVAL_IS_NULL (EJS_ARRAY_ELEMENTS(_this)[i])) {
                rv = i;
                break;
            }
        }
    }
    else {
        for (i = 0; i < EJS_ARRAY_LEN(_this); i ++) {
            if (EJSVAL_TO_BOOLEAN(_ejs_op_strict_eq (needle, EJS_ARRAY_ELEMENTS(_this)[i]))) {
                rv = i;
                break;
            }
        }
    }

    return NUMBER_TO_EJSVAL (rv);
}

static ejsval
_ejs_Array_isArray (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    if (argc == 0 || EJSVAL_IS_NULL(args[0]) || !EJSVAL_IS_OBJECT(args[0]))
        return _ejs_false;

    return BOOLEAN_TO_EJSVAL (!strcmp (CLASSNAME(EJSVAL_TO_OBJECT(args[0])), "Array"));
}

void
_ejs_array_init(ejsval global)
{
    START_SHADOW_STACK_FRAME;

    _ejs_sparsearray_specops =  _ejs_object_specops;
    _ejs_sparsearray_specops.class_name = "Array";

    _ejs_gc_add_named_root (_ejs_Array_proto);
    _ejs_Array_proto = _ejs_object_new(_ejs_null, &_ejs_object_specops);

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new (_ejs_null, _ejs_atom_Array, (EJSClosureFunc)_ejs_Array_impl));
    _ejs_Array = tmpobj;

    _ejs_object_setprop (_ejs_Array,       _ejs_atom_prototype,  _ejs_Array_proto);

#define OBJ_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_Array, EJS_STRINGIFY(x), _ejs_Array_##x)
#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_Array_proto, EJS_STRINGIFY(x), _ejs_Array_prototype_##x)

    OBJ_METHOD(isArray);

    PROTO_METHOD(push);
    PROTO_METHOD(pop);
    PROTO_METHOD(shift);
    PROTO_METHOD(unshift);
    PROTO_METHOD(concat);
    PROTO_METHOD(slice);
    PROTO_METHOD(splice);
    PROTO_METHOD(indexOf);
    PROTO_METHOD(join);
    PROTO_METHOD(forEach);

#undef OBJ_METHOD
#undef PROTO_METHOD

    _ejs_object_setprop (global,           _ejs_atom_Array,      _ejs_Array);

    END_SHADOW_STACK_FRAME;
}

static ejsval
_ejs_array_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr)
{
    // check if propertyName is an integer, or a string that we can convert to an int
    EJSBool is_index = EJS_FALSE;
    int idx = 0;
    if (!isCStr && EJSVAL_IS_NUMBER(propertyName)) {
        double n = EJSVAL_TO_NUMBER(propertyName);
        if (floor(n) == n) {
            idx = (int)n;
            is_index = EJS_TRUE;
        }
    }

    if (is_index) {
        if (idx < 0 || idx > EJS_ARRAY_LEN(obj)) {
            printf ("getprop(%d) on an array, returning undefined\n", idx);
            return _ejs_undefined;
        }
        return EJS_ARRAY_ELEMENTS(obj)[idx];
    }

    // we also handle the length getter here
    if ((isCStr && !strcmp("length", (char*)EJSVAL_TO_PRIVATE_PTR_IMPL(propertyName)))
        || (!isCStr && EJSVAL_IS_STRING(propertyName) && !strcmp ("length", EJSVAL_TO_FLAT_STRING(propertyName)))) {
        return NUMBER_TO_EJSVAL (EJS_ARRAY_LEN(obj));
    }

    // otherwise we fallback to the object implementation
    return _ejs_object_specops.get (obj, propertyName, isCStr);
}

static EJSPropertyDesc*
_ejs_array_specop_get_own_property (ejsval obj, ejsval propertyName)
{
    if (EJSVAL_IS_NUMBER(propertyName)) {
        double needle = EJSVAL_TO_NUMBER(propertyName);
        int needle_int;
        if (EJSDOUBLE_IS_INT32(needle, &needle_int)) {
            if (needle_int >= 0 && needle_int < EJS_ARRAY_LEN(obj))
                return NULL; // XXX
        }
            
    }
    return _ejs_object_specops.get_own_property (obj, propertyName);
}

static EJSPropertyDesc*
_ejs_array_specop_get_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get_property (obj, propertyName);
}

static void
_ejs_array_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag)
{
    // check if propertyName is a uint32, or a string that we can convert to an uint32
    int idx = -1;
    if (EJSVAL_IS_NUMBER(propertyName)) {
        double n = EJSVAL_TO_NUMBER(propertyName);
        if (floor(n) == n) {
            idx = (int)n;
        }
    }

    if (idx != -1) {
        if (idx >= EJS_ARRAY_ALLOC(obj)) {
            int new_alloc = idx + 10;
            ejsval* new_elements = (ejsval*)malloc (sizeof(ejsval*) * new_alloc);
            memmove (new_elements, EJS_ARRAY_ELEMENTS(obj), EJS_ARRAY_ALLOC(obj) * sizeof(ejsval));
            free (EJS_ARRAY_ELEMENTS(obj));
            EJS_ARRAY_ELEMENTS(obj) = new_elements;
            EJS_ARRAY_ALLOC(obj) = new_alloc;
        }
        EJS_ARRAY_ELEMENTS(obj)[idx] = val;
        EJS_ARRAY_LEN(obj) = idx + 1;
        if (EJS_ARRAY_LEN(obj) >= EJS_ARRAY_ALLOC(obj))
            abort();
        return;
    }
    // if we fail there, we fall back to the object impl below

    _ejs_object_specops.put (obj, propertyName, val, flag);
}

static EJSBool
_ejs_array_specop_can_put (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.can_put (obj, propertyName);
}

static EJSBool
_ejs_array_specop_has_property (ejsval obj, ejsval propertyName)
{
    // check if propertyName is a uint32, or a string that we can convert to an uint32
    int idx = -1;
    if (EJSVAL_IS_NUMBER(propertyName)) {
        double n = EJSVAL_TO_NUMBER(propertyName);
        if (floor(n) == n) {
            idx = (int)n;

            return idx > 0 && idx < EJS_ARRAY_LEN(obj);
        }
    }

    // if we fail there, we fall back to the object impl below

    return _ejs_object_specops.has_property (obj, propertyName);
}

static EJSBool
_ejs_array_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag)
{
    return _ejs_object_specops._delete (obj, propertyName, flag);
}

static ejsval
_ejs_array_specop_default_value (ejsval obj, const char *hint)
{
    return _ejs_object_specops.default_value (obj, hint);
}

static EJSBool
_ejs_array_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag)
{
    return _ejs_object_specops.define_own_property (obj, propertyName, propertyDescriptor, flag);
}

static EJSObject*
_ejs_array_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSArray);
}


static void
_ejs_array_specop_finalize (EJSObject* obj)
{
    free (((EJSArray*)obj)->elements);
    _ejs_object_specops.finalize (obj);
}

static void
_ejs_array_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    _ejs_array_foreach_element ((EJSArray*)obj, scan_func);
    _ejs_object_specops.scan (obj, scan_func);
}

