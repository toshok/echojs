/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <string.h>
#include <math.h>

#include "ejs.h"
#include "ejs-ops.h"
#include "ejs-array.h"
#include "ejs-function.h"
#include "ejs-proxy.h"
#include "ejs-string.h"
#include "ejs-error.h"
#include "ejs-symbol.h"

// num > SPARSE_ARRAY_CUTOFF in "Array($num)" or "new Array($num)" triggers a sparse array
#define SPARSE_ARRAY_CUTOFF 50000

#define _EJS_ARRAY_LEN(arrobj)      (((EJSArray*)arrobj)->array_length)
#define _EJS_ARRAY_ELEMENTS(arrobj) (((EJSArray*)arrobj)->elements)

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


ejsval
_ejs_array_new (int numElements, EJSBool fill)
{
    EJSArray* rv = _ejs_gc_new (EJSArray);

    if (numElements > SPARSE_ARRAY_CUTOFF) {
        _ejs_init_object ((EJSObject*)rv, _ejs_Array_prototype, &_ejs_sparsearray_specops);

        rv->sparse.arraylet_alloc = 5;
        rv->sparse.arraylet_num = 0;
        rv->sparse.arraylets = (Arraylet*)calloc(rv->sparse.arraylet_alloc, sizeof(Arraylet));
    }
    else {
        _ejs_init_object ((EJSObject*)rv, _ejs_Array_prototype, &_ejs_Array_specops);

        rv->dense.array_alloc = numElements + 5;
        rv->dense.elements = (ejsval*)malloc(rv->dense.array_alloc * sizeof (ejsval));
        for (int i = 0; i < numElements; i ++)
            rv->dense.elements[i] = MAGIC_TO_EJSVAL_IMPL(EJS_ARRAY_HOLE);
    }

    rv->array_length = numElements;
    _ejs_property_desc_set_writable (&rv->array_length_desc, EJS_TRUE);
    _ejs_property_desc_set_value (&rv->array_length_desc, NUMBER_TO_EJSVAL(numElements));

    return OBJECT_TO_EJSVAL(rv);
}

ejsval
_ejs_array_new_copy (int numElements, ejsval *elements)
{
    ejsval arr = _ejs_array_new (0, EJS_FALSE);
    _ejs_array_push_dense (arr, numElements, elements);
    return arr;
}

static void
maybe_realloc_dense (EJSArray *arr, int amount_to_add)
{
    if (arr->array_length + amount_to_add > arr->dense.array_alloc) {
        int new_alloc = arr->array_length + amount_to_add + 32;
        ejsval* new_elements = (ejsval*)malloc(new_alloc * sizeof(ejsval));
        memmove (new_elements, arr->dense.elements, arr->array_length * sizeof(ejsval));
        free (arr->dense.elements);
        arr->dense.elements = new_elements;
        arr->dense.array_alloc = new_alloc;
    }
}

uint32_t
_ejs_array_push_dense(ejsval array, int argc, ejsval *args)
{
    EJSArray *arr = (EJSArray*)EJSVAL_TO_OBJECT(array);
    maybe_realloc_dense (arr, argc);
    for (int i = 0; i < argc; i ++) {
        EJSDENSEARRAY_ELEMENTS(arr)[EJSARRAY_LEN(arr)++] = args[i];
    }
    return EJSARRAY_LEN(arr);
}

ejsval
_ejs_array_pop_dense(ejsval array)
{
    EJSArray *arr = (EJSArray*)EJSVAL_TO_OBJECT(array);
    if (EJSARRAY_LEN(arr) == 0)
        return _ejs_undefined;
    return EJSDENSEARRAY_ELEMENTS(arr)[--EJSARRAY_LEN(arr)];
}

ejsval _ejs_Array_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_Array EJSVAL_ALIGNMENT;

static ejsval
_ejs_Array_impl (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    if (EJSVAL_IS_UNDEFINED(_this)) {
        // called as a function
        if (argc == 0) {
            return _ejs_array_new(0, EJS_FALSE);
        }
        else if (argc == 1 && EJSVAL_IS_NUMBER(args[0])) {
            uint32_t alloc = ToUint32(args[0]);
            return _ejs_array_new(alloc, EJS_TRUE);
        }
        else {
            return _ejs_array_new_copy (argc, args);
        }
    }
    else {
        // called as a constructor
        EJSArray* arr = (EJSArray*)EJSVAL_TO_OBJECT(_this);

        if (argc == 0) {
            int alloc = 5;

            arr->array_length = 0;
            arr->dense.array_alloc = alloc;
            arr->dense.elements = (ejsval*)calloc(arr->dense.array_alloc, sizeof (ejsval));
        }
        else if (argc == 1 && EJSVAL_IS_NUMBER(args[0])) {
            int alloc = ToUint32(args[0]);
            if (alloc > SPARSE_ARRAY_CUTOFF) {
                arr->obj.ops = &_ejs_Array_specops;

                arr->sparse.arraylet_alloc = 5;
                arr->sparse.arraylet_num = 0;
                arr->sparse.arraylets = (Arraylet*)calloc(arr->sparse.arraylet_alloc, sizeof(Arraylet));
            }
            else {
                arr->dense.array_alloc = alloc;
                arr->dense.elements = (ejsval*)calloc(arr->dense.array_alloc, sizeof (ejsval));
            }
            arr->array_length = alloc;
        }
        else {
            arr->array_length = argc;
            arr->dense.array_alloc = argc;
            arr->dense.elements = (ejsval*)malloc(arr->dense.array_alloc * sizeof (ejsval));

            for (int i = 0; i < argc; i ++)
                arr->dense.elements[i] = args[i];
        }


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
        ejsval first = EJS_DENSE_ARRAY_ELEMENTS(_this)[0];
        memmove (EJS_DENSE_ARRAY_ELEMENTS(_this), EJS_DENSE_ARRAY_ELEMENTS(_this) + 1, sizeof(ejsval) * len-1);
        EJS_ARRAY_LEN(_this) --;
        return first;
    }

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    ejsval O = ToObject(_this);
    EJSObject* Oobj = EJSVAL_TO_OBJECT(O);

    // 2. Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    ejsval lenVal = OP(Oobj,get) (O, _ejs_atom_length, O);

    // 3. Let len be ToUint32(lenVal).
    int len = ToUint32(lenVal);

    // 4. If len is zero, then
    if (len == 0) {
        //   a. Call the [[Put]] internal method of O with arguments "length", 0, and true.
        OP(Oobj,put) (O, _ejs_atom_length, NUMBER_TO_EJSVAL(0), O, EJS_TRUE);
        //   b. Return undefined.
        return _ejs_undefined;
    }

    // 5. Let first be the result of calling the [[Get]] internal method of O with argument "0".
    ejsval first = OP(Oobj,get) (O, _ejs_string_new_utf8 ("0"), O);

    // 6. Let k be 1.
    int k = 1;
    // 7. Repeat, while k < len
    while (k < len) {
        //   a. Let from be ToString(k).
        ejsval from = ToString(NUMBER_TO_EJSVAL(k));

        //   b. Let to be ToString(k–1).
        ejsval to = ToString(NUMBER_TO_EJSVAL(k-1));

        //   c. Let fromPresent be the result of calling the [[HasProperty]] internal method of O with argument from.
        EJSBool fromPresent = OP(Oobj,has_property)(O, from);

        //   d. If fromPresent is true, then
        if (fromPresent) {
            //     i. Let fromVal be the result of calling the [[Get]] internal method of O with argument from.
            ejsval fromVal = OP(Oobj,get)(O, from, O);

            //     ii. Call the [[Put]] internal method of O with arguments to, fromVal, and true.
            OP(Oobj,put)(O, to, fromVal, O, EJS_TRUE);
        }
        //   e. Else, fromPresent is false
        else {
            //     i. Call the [[Delete]] internal method of O with arguments to and true.
            OP(Oobj,_delete)(O, to, EJS_TRUE);
        }
        //   f. Increase k by 1.
        k++;
    }
    // 8. Call the [[Delete]] internal method of O with arguments ToString(len–1) and true.
    OP(Oobj,_delete) (O, NUMBER_TO_EJSVAL(len-1), EJS_TRUE);

    // 9. Call the [[Put]] internal method of O with arguments "length", (len–1) , and true.
    OP(Oobj,put) (O, _ejs_atom_length, NUMBER_TO_EJSVAL(len-1), O, EJS_TRUE);
    
    // 10. Return first.
    return first;
}

static ejsval
_ejs_Array_prototype_unshift (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    // EJS fast path for arrays
    if (EJSVAL_IS_DENSE_ARRAY(_this)) {
        EJSArray *arr = (EJSArray*)EJSVAL_TO_OBJECT(_this);
        maybe_realloc_dense (arr, argc);
        int len = EJS_ARRAY_LEN(_this);
        memmove (EJS_DENSE_ARRAY_ELEMENTS(_this) + argc, EJS_DENSE_ARRAY_ELEMENTS(_this), sizeof(ejsval) * len);
        memmove (EJS_DENSE_ARRAY_ELEMENTS(_this), args, sizeof(ejsval) * argc);
        EJS_ARRAY_LEN(_this) += argc;
        return NUMBER_TO_EJSVAL(len + argc);
    }

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    ejsval O = ToObject(_this);
    EJSObject* Oobj = EJSVAL_TO_OBJECT(O);

    // 2. Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    ejsval lenVal = OP(Oobj,get)(O, _ejs_atom_length, O);

    // 3. Let len be ToUint32(lenVal).
    uint32_t len = ToUint32(lenVal);

    // 4. Let argCount be the number of actual arguments.
    uint32_t argCount = argc;

    // 5. Let k be len.
    uint32_t k = len;

    // 6. Repeat, while k > 0,
    while (k > 0) {
        //   a. Let from be ToString(k–1).
        ejsval from = ToString(NUMBER_TO_EJSVAL(k-1));

        //   b. Let to be ToString(k+argCount –1).
        ejsval to = ToString(NUMBER_TO_EJSVAL(k+argCount - 1));

        //   c. Let fromPresent be the result of calling the [[HasProperty]] internal method of O with argument from.
        EJSBool fromPresent = OP(Oobj,has_property)(O, from);

        //   d. If fromPresent is true, then
        if (fromPresent) {
            //      i. Let fromValue be the result of calling the [[Get]] internal method of O with argument from.
            ejsval fromValue = OP(Oobj,get)(O, from, O);

            //      ii. Call the [[Put]] internal method of O with arguments to, fromValue, and true.
            OP(Oobj,put)(O, to, fromValue, O, EJS_TRUE);
        }
        //   e. Else, fromPresent is false
        else {
            //      i. Call the [[Delete]] internal method of O with arguments to, and true.
            OP(Oobj,_delete)(O, to, EJS_TRUE);
        }
        //   f. Decrease k by 1.
        k--;
    }
    // 7. Let j be 0.
    uint32_t j = 0;

    // 8. Let items be an internal List whose elements are, in left to right order, the arguments that were passed to this function invocation.
    // 9. Repeat, while items is not empty
    for (int i = 0; i < argc; i ++) {
        //   a. Remove the first element from items and let E be the value of that element.
        ejsval E = args[i];

        //   b. Call the [[Put]] internal method of O with arguments ToString(j), E, and true.
        OP(Oobj,put)(O, ToString(NUMBER_TO_EJSVAL(j)), E, O, EJS_TRUE);
        //   c. Increase j by 1.
        j++;
    }

    // 10. Call the [[Put]] internal method of O with arguments "length", len+argCount, and true.
    OP(Oobj,put)(O, _ejs_atom_length, NUMBER_TO_EJSVAL(len+argCount), O, EJS_TRUE);
    // 11. Return len+argCount.
    return NUMBER_TO_EJSVAL(len+argCount);
}

// ECMA262: 15.4.4.9
static ejsval
_ejs_Array_prototype_reverse (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    ejsval O = ToObject(_this);

    // 2. Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    ejsval lenVal = OP(EJSVAL_TO_OBJECT(O),get)(O, _ejs_atom_length, O);
    // 3. Let len be ToUint32(lenVal).
    uint32_t len = ToUint32(lenVal);

    // 4. Let middle be floor(len/2). 
    uint32_t middle = len / 2;

    // 5. Let lower be 0. 
    uint32_t lower = 0;

    // 6. Repeat, while lower != middle 
    while (lower != middle) {
        //    a. Let upper be len - lower - 1. 
        uint32_t upper = len - lower - 1;

        //    b. Let upperP be ToString(upper). 
        ejsval upperP = ToString(NUMBER_TO_EJSVAL(upper));

        //    c. Let lowerP be ToString(lower). 
        ejsval lowerP = ToString(NUMBER_TO_EJSVAL(lower));

        //    d. Let lowerValue be the result of calling the [[Get]] internal method of O with argument lowerP. 
        ejsval lowerValue = OP(EJSVAL_TO_OBJECT(O),get)(O, lowerP, O);

        //    e. Let upperValue be the result of calling the [[Get]] internal method of O with argument upperP . 
        ejsval upperValue = OP(EJSVAL_TO_OBJECT(O),get)(O, upperP, O);

        //    f. Let lowerExists be the result of calling the [[HasProperty]] internal method of O with argument lowerP. 
        EJSBool lowerExists = OP(EJSVAL_TO_OBJECT(O),has_property)(O, lowerP);

        //    g. Let upperExists be the result of calling the [[HasProperty]] internal method of O with argument upperP. 
        EJSBool upperExists = OP(EJSVAL_TO_OBJECT(O),has_property)(O, upperP);

        //    h. If lowerExists is true and upperExists is true, then 
        if (lowerExists && upperExists) {
            //       i. Call the [[Put]] internal method of O with arguments lowerP, upperValue, and true . 
            OP(EJSVAL_TO_OBJECT(O),put)(O, lowerP, upperValue, O, EJS_TRUE);

            //       ii. Call the [[Put]] internal method of O with arguments upperP, lowerValue, and true . 
            OP(EJSVAL_TO_OBJECT(O),put)(O, upperP, lowerValue, O, EJS_TRUE);
        }
        //    i. Else if lowerExists is false and upperExists is true, then 
        else if (!lowerExists && upperExists) {
            //       i. Call the [[Put]] internal method of O with arguments lowerP, upperValue, and true . 
            OP(EJSVAL_TO_OBJECT(O),put)(O, lowerP, upperValue, O, EJS_TRUE);
            //       ii. Call the [[Delete]] internal method of O, with arguments upperP and true. 
            OP(EJSVAL_TO_OBJECT(O),_delete)(O, upperP, EJS_TRUE);
        }
        //    j. Else if lowerExists is true and upperExists is false, then 
        else if (lowerExists && !upperExists) {
            //       i. Call the [[Delete]] internal method of O, with arguments lowerP and true . 
            OP(EJSVAL_TO_OBJECT(O),_delete)(O, lowerP, EJS_TRUE);
            //       ii. Call the [[Put]] internal method of O with arguments upperP, lowerValue, and true . 
            OP(EJSVAL_TO_OBJECT(O),put)(O, upperP, lowerValue, O, EJS_TRUE);
        }
        //    k. Else, both lowerExists and upperExists are false 
        //       i. No action is required. 

        //    l. Increase lower by 1. 
        lower ++;
    }
    // 7. Return O
    return O;
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

    // 2. Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    ejsval lenVal = OP(EJSVAL_TO_OBJECT(O),get)(O, _ejs_atom_length, O);
    // 3. Let n be ToUint32(lenVal).
    uint32_t n = ToUint32(lenVal);

    // 4. Let items be an internal List whose elements are, in left to right order, the arguments that were passed to this 
    //    function invocation.
    ejsval* items = args;

    // 5. Repeat, while items is not empty
    for (int i = 0; i < argc; i ++) {
        //    a. Remove the first element from items and let E be the value of the element.
        ejsval E = items[i];

        //    b. Call the [[Put]] internal method of O with arguments ToString(n), E, and true.
        OP(EJSVAL_TO_OBJECT(O),put)(O, ToString(NUMBER_TO_EJSVAL(n)), E, O, EJS_TRUE);
        //    c. Increase n by 1.
        n++;
    }
    // 6. Call the [[Put]] internal method of O with arguments "length", n, and true.
    OP(EJSVAL_TO_OBJECT(O),put)(O, _ejs_atom_length, NUMBER_TO_EJSVAL(n), O, EJS_TRUE);

    // 7. Return n.
    return NUMBER_TO_EJSVAL(n);
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
    ejsval lenVal = OP(EJSVAL_TO_OBJECT(O),get)(O, _ejs_atom_length, O);

    // 3. Let len be ToUint32(lenVal).
    uint32_t len = ToUint32(lenVal);

    // 4. If len is zero, 
    if (len == 0) {
        //    a. Call the [[Put]] internal method of O with arguments "length", 0, and true.
        OP(EJSVAL_TO_OBJECT(O),put)(O, _ejs_atom_length, _ejs_zero, O, EJS_TRUE);
        // EJS: why is this done? to overwrite a accessor property?

        //    b. Return undefined.
        return _ejs_undefined;
    }
    // 5. Else, len > 0
    else {
        //    a. Let indx be ToString(len–1).
        ejsval indx = ToString(NUMBER_TO_EJSVAL(len-1));

        //    b. Let element be the result of calling the [[Get]] internal method of O with argument indx.
        ejsval element = OP(EJSVAL_TO_OBJECT(O),get)(O, indx, O);
        
        //    c. Call the [[Delete]] internal method of O with arguments indx and true.
        OP(EJSVAL_TO_OBJECT(O),_delete)(O, indx, EJS_TRUE);

        //    d. Call the [[Put]] internal method of O with arguments "length", indx, and true.
        // EJS: both node 0.10.21 and firefox nightly change length to a number here, not 'index' which is a string.
        OP(EJSVAL_TO_OBJECT(O),put)(O, _ejs_atom_length, NUMBER_TO_EJSVAL(len-1), O, EJS_TRUE);

        //    e. Return element.
        return element;
    }
}

// ECMA262: 15.4.4.4
static ejsval
_ejs_Array_prototype_concat (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    int numElements;

    numElements = EJS_ARRAY_LEN(_this); // we need to verify that we haven't been call'ed on something...
    for (int i = 0; i < argc; i ++) {
        if (EJSVAL_IS_ARRAY(args[i]))
            numElements += EJS_ARRAY_LEN(args[i]);
        else
            numElements += 1;
    }

    ejsval rv = _ejs_array_new(numElements, EJS_FALSE);
    numElements = 0;

    memmove (EJS_DENSE_ARRAY_ELEMENTS(rv) + numElements, EJS_DENSE_ARRAY_ELEMENTS(_this), sizeof(ejsval) * EJS_ARRAY_LEN(_this));
    numElements += EJS_ARRAY_LEN(_this);
    for (int i = 0; i < argc; i ++) {
        if (EJSVAL_IS_ARRAY(args[i])) {
            memmove (EJS_DENSE_ARRAY_ELEMENTS(rv) + numElements, EJS_DENSE_ARRAY_ELEMENTS(args[i]), sizeof(ejsval) * EJS_ARRAY_LEN(args[i]));
            numElements += EJS_ARRAY_LEN(args[i]);
        }
        else {
            EJS_DENSE_ARRAY_ELEMENTS(rv)[numElements] = args[i];
            numElements ++;
        }
    }
    EJS_ARRAY_LEN(rv) = numElements;

    return rv;
}

static ejsval
_ejs_array_slice_dense (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    int len = EJS_ARRAY_LEN(_this);
    int begin = argc > 0 ? (int)EJSVAL_TO_NUMBER(args[0]) : 0;
    int end = argc > 1 ? (int)EJSVAL_TO_NUMBER(args[1]) : len;

    begin = MIN(begin, len);
    end = MIN(end, len);

    ejsval rv = _ejs_array_new(end-begin, EJS_FALSE);

    memmove (&EJS_DENSE_ARRAY_ELEMENTS(rv)[0],
             &EJS_DENSE_ARRAY_ELEMENTS(_this)[begin],
             (end-begin) * sizeof(ejsval));

    return rv;
}

// ECMA262: 15.4.4.10
static ejsval
_ejs_Array_prototype_slice (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    if (EJSVAL_IS_DENSE_ARRAY(_this)) {
        return _ejs_array_slice_dense(env, _this, argc, args);
    }

    ejsval start = _ejs_undefined;
    ejsval end = _ejs_undefined;

    if (argc > 0) start = args[0];
    if (argc > 1) end = args[1];

    /* 1. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);

    /* 2. Let A be a new array created as if by the expression new Array() where Array is the standard built-in  */
    /*    constructor with that name. */
    ejsval A = _ejs_array_new(0, EJS_FALSE);

    /* 3. Let lenVal be the result of calling the [[Get]] internal method of O with argument "length". */
    ejsval lenVal = _ejs_object_getprop (O, _ejs_atom_length);

    /* 4. Let len be ToUint32(lenVal). */
    uint32_t len = ToUint32(lenVal);

    /* 5. Let relativeStart be ToInteger(start). */
    int32_t relativeStart = ToInteger(start);

    /* 6. If relativeStart is negative, let k be max((len + relativeStart),0); else let k be min(relativeStart, len). */
    int32_t k = (relativeStart < 0) ? MAX(len + relativeStart, 0) : MIN(relativeStart, len);

    /* 7. If end is undefined, let relativeEnd be len; else let relativeEnd be ToInteger(end). */
    int32_t relativeEnd = (EJSVAL_IS_UNDEFINED(end)) ? len : ToInteger(end);
        
    /* 8. If relativeEnd is negative, let final be max((len + relativeEnd),0); else let final be min(relativeEnd, len). */
    int32_t final = relativeEnd < 0 ? MAX(len + relativeEnd, 0) : MIN(relativeEnd, len);

    /* 9. Let n be 0. */
    int n = 0;

    /* 10. Repeat, while k < final */
    while (k < final) {
        /* a. Let Pk be ToString(k). */
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));

        /* b. Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk. */
        EJSBool kPresent = OP(EJSVAL_TO_OBJECT(O),has_property)(O, Pk);

        /* c. If kPresent is true, then */
        if (kPresent) {
            /*    i. Let kValue be the result of calling the [[Get]] internal method of O with argument Pk. */
            ejsval kValue = OP(EJSVAL_TO_OBJECT(O),get)(O, Pk, O);

            /*    ii. Call the [[DefineOwnProperty]] internal method of A with arguments ToString(n), Property  */
            /*        Descriptor {[[Value]]: kValue, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]:  */
            /*        true}, and false. */
            _ejs_object_setprop (A, NUMBER_TO_EJSVAL(n), kValue);
        }

        /* d. Increase k by 1. */
        k++;
        /* e. Increase n by 1. */
        n++;
    }

    /* 11. Return A. */
    return A;
}

// ECMA262: 15.4.4.5
static ejsval
_ejs_Array_prototype_join (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    // XXX this method should be optimized to create a rope
    // instead of a flat string.

    if (EJS_ARRAY_LEN(_this) == 0)
        return _ejs_atom_empty;

    if (!EJSVAL_IS_DENSE_ARRAY(_this))
        abort();

    const jschar* separator;
    int separator_len;

    if (argc > 0) {
        ejsval sepToString = ToString(args[0]);
        separator_len = EJSVAL_TO_STRLEN(sepToString);
        separator = EJSVAL_TO_FLAT_STRING(sepToString);
    }
    else {
        static jschar comma[] = { (jschar)',' };
        separator = comma;
        separator_len = 1;
    }
  
    jschar* result;
    int result_len = 0;

    int num_strings = EJS_ARRAY_LEN(_this);

    ejsval* strings = (ejsval*)malloc (sizeof (ejsval) * num_strings);
    int i;

    for (i = 0; i < num_strings; i ++) {
        strings[i] = ToString(EJS_DENSE_ARRAY_ELEMENTS(_this)[i]);
        result_len += EJSVAL_TO_STRLEN(strings[i]);
    }

    result_len += separator_len * (EJS_ARRAY_LEN(_this)-1) + 1/* \0 terminator */;

    result = (jschar*)malloc (sizeof(jschar) * result_len);
    jschar *p = result;

    for (i = 0; i < num_strings; i ++) {
        jschar *str_str = EJSVAL_TO_FLAT_STRING(strings[i]);
        int slen = EJSVAL_TO_STRLEN(strings[i]);
        memmove (p, str_str, slen * sizeof(jschar));
        p += slen;
        if (i < num_strings-1) {
            memmove (p, separator, separator_len * sizeof(jschar));
            p += separator_len;
        }
    }
    *p = 0;

    ejsval rv = _ejs_string_new_ucs2(result);

    free (result);
    free (strings);

    return rv;
}

ejsval
_ejs_array_join (ejsval array, ejsval sep)
{
    return _ejs_Array_prototype_join (_ejs_null, array, 1, &sep);
}

ejsval
_ejs_Array_prototype_toString (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    return _ejs_Array_prototype_join (env, _this, 0, NULL);
}

// ECMA262: 15.4.4.15
// Array.prototype.lastIndexOf ( searchElement [ , fromIndex ] )
static ejsval
_ejs_Array_prototype_lastIndexOf (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval searchElement = _ejs_undefined;

    if (argc > 0) searchElement = args[0];

    /* 1. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);
    EJSObject* Oobj = EJSVAL_TO_OBJECT(O);

    /* 2. Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length". */
    ejsval lenValue = OP(Oobj,get)(O, _ejs_atom_length, O);

    /* 3. Let len be ToUint32(lenValue). */
    uint32_t len = ToUint32(lenValue);

    /* 4. If len is 0, return -1. */
    if (len == 0) return NUMBER_TO_EJSVAL(-1);

    int32_t n;
    /* 5. If argument fromIndex was passed let n be ToInteger(fromIndex); else let n be len. */
    if (argc > 1)
        n = ToInteger(args[1]);
    else
        n = len;

    int k;

    /* 6. If n ≥ 0, then let k be min(n, len – 1). */
    if (n >= 0)
        k = min(n, len-1);
    /* 7. Else, n < 0 */
    else
    /*    a. Let k be len - abs(n). */
        k = len - abs(n);

    /* 8. Repeat, while k≥ 0 */
    while (k >= 0) {
        ejsval kstr = ToString(NUMBER_TO_EJSVAL(k));

        /*    a. Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument ToString(k). */
        EJSBool kPresent = OP(Oobj,has_property)(O, kstr);
        /*    b. If kPresent is true, then */
        if (kPresent) {
            /*       i. Let elementK be the result of calling the [[Get]] internal method of O with the argument ToString(k). */
            ejsval elementK = OP(Oobj,get)(O, kstr, O);
            /*       ii. Let same be the result of applying the Strict Equality Comparision Algorithm to searchElement and elementK. */
            ejsval same = _ejs_op_strict_eq (searchElement, elementK);

            /*       iii. If same is true, return k. */
            if (EJSVAL_TO_BOOLEAN(same)) return NUMBER_TO_EJSVAL(k);
        }
        /*    c. Decrease k by 1. */
        k--;
    }
    /* 9. Return -1. */
    return NUMBER_TO_EJSVAL(-1);
}


// ECMA262: 15.4.4.16
// Array.prototype.every ( callbackfn [ , thisArg ] )
static ejsval
_ejs_Array_prototype_every (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval callbackfn = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc > 0) callbackfn = args[0];
    if (argc > 1) thisArg = args[1];

    /* 1. Let O be the result of calling ToObject passing the this value as the argument.  */
    ejsval O = ToObject(_this);
    EJSObject* Oobj = EJSVAL_TO_OBJECT(O);

    /* 2. Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".  */
    ejsval lenValue = OP(Oobj,get)(O, _ejs_atom_length, O);
    /* 3. Let len be ToUint32(lenValue).  */
    uint32_t len = ToUint32(lenValue);

    /* 4. If IsCallable(callbackfn) is false, throw a TypeError exception.  */
    if (!EJSVAL_IS_CALLABLE(callbackfn))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "callback function is not a function");


    /* 5. If thisArg was supplied, let T be thisArg; else let T be undefined.  */
    ejsval T = thisArg;

    /* 6. Let k be 0.  */
    uint32_t k = 0;

    /* 7. Repeat, while k < len  */
    while (k < len) {
        /*    a. Let Pk be ToString(k).  */
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));

        /*    b. Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.  */
        EJSBool kPresent = OP(Oobj,has_property)(O, Pk);

        /*    c. If kPresent is true, then  */
        if (kPresent) {
            /*       i. Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.  */
            ejsval kValue = OP(Oobj,get)(O, Pk, O);

            /*       ii. Let testResult be the result of calling the [[Call]] internal method of callbackfn with T as the  */
            /*           this value and argument list containing kValue, k, and O.  */
            ejsval callbackargs[3] = {
                kValue,
                NUMBER_TO_EJSVAL(k),
                O
            };
            ejsval testResult = _ejs_invoke_closure (callbackfn, T, 3, callbackargs);

            /*       iii. If ToBoolean(testResult) is false, return false.  */
            if (!EJSVAL_TO_BOOLEAN(ToBoolean(testResult)))
                return _ejs_false;
        }

        /*    d. Increase k by 1.  */
        k++;
    }

    /* 8. Return true. */
    return _ejs_true;
}

// ECMA262: 15.4.4.17
// Array.prototype.some ( callbackfn [ , thisArg ] ) 
static ejsval
_ejs_Array_prototype_some (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    ejsval callbackfn = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc > 0) callbackfn = args[0];
    if (argc > 1) thisArg = args[1];

    /* 1. Let O be the result of calling ToObject passing the this value as the argument.  */
    ejsval O = ToObject(_this);
    EJSObject* Oobj = EJSVAL_TO_OBJECT(O);

    /* 2. Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".  */
    ejsval lenValue = OP(Oobj,get)(O, _ejs_atom_length, O);
    /* 3. Let len be ToUint32(lenValue).  */
    uint32_t len = ToUint32(lenValue);

    /* 4. If IsCallable(callbackfn) is false, throw a TypeError exception.  */
    if (!EJSVAL_IS_CALLABLE(callbackfn))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "callback function is not a function");

    /* 5. If thisArg was supplied, let T be thisArg; else let T be undefined.  */
    ejsval T = thisArg;


    /* 6. Let k be 0.  */
    uint32_t k = 0;

    /* 7. Repeat, while k < len  */
    while (k < len) {
        /*    a. Let Pk be ToString(k).  */
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));

        /*    b. Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.  */
        EJSBool kPresent = OP(Oobj,has_property)(O, Pk);

        /*    c. If kPresent is true, then  */
        if (kPresent) {
            /*       i. Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.  */
            ejsval kValue = OP(Oobj,get)(O, Pk, O);

            /*       ii. Let testResult be the result of calling the [[Call]] internal method of callbackfn with T as the  */
            /*           this value and argument list containing kValue, k, and O.  */
            ejsval callbackargs[3] = {
                kValue,
                NUMBER_TO_EJSVAL(k),
                O
            };
            ejsval testResult = _ejs_invoke_closure (callbackfn, T, 3, callbackargs);

            /*       iii. If ToBoolean(testResult) is true, return true.  */
            if (EJSVAL_TO_BOOLEAN(ToBoolean(testResult)))
                return _ejs_true;
        }

        /*    d. Increase k by 1.  */
        k++;
    }

    /* 8. Return false. */
    return _ejs_false;
}

// ECMA262: 15.4.4.18
static ejsval
_ejs_Array_prototype_forEach (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    ejsval callbackfn = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc >= 1)
        callbackfn = args[0];

    if (argc >= 2)
        thisArg = args[1];

    if (EJSVAL_IS_NULL_OR_UNDEFINED(_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Array.prototype.forEach called on null or undefined");

    /* 1. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);

    /* 4. If IsCallable(callbackfn) is false, throw a TypeError exception. */
    if (!EJSVAL_IS_CALLABLE(callbackfn))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "argument is not a function");

    ejsval foreach_args[3];
    if (EJSVAL_IS_DENSE_ARRAY(O)) {
        int i;
        foreach_args[2] = O;
        for (i = 0; i < EJS_ARRAY_LEN(_this); i ++) {
            foreach_args[0] = EJS_DENSE_ARRAY_ELEMENTS(_this)[i];
            foreach_args[1] = NUMBER_TO_EJSVAL(i);
            _ejs_invoke_closure (callbackfn, thisArg, 3, foreach_args);
        }
    }
    else {
        /* 2. Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".*/
        ejsval lenVal = _ejs_object_getprop (O, _ejs_atom_length);
        
        /* 3. Let len be ToUint32(lenValue). */
        uint32_t len = ToUint32(lenVal);

        /* 5. If thisArg was supplied, let T be thisArg; else let T be undefined. */
        // we already set thisArg appropriately

        /* 6. Let k be 0. */
        uint32_t k = 0;

        /* 7. Repeat, while k < len */
        while (k < len) {
            /* a. Let Pk be ToString(k). */
            ejsval Pk = ToString (NUMBER_TO_EJSVAL(k));

            /* b. Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk. */
            EJSBool kPresent = OP(EJSVAL_TO_OBJECT(O), has_property)(O, Pk);

            /* c. If kPresent is true, then */
            if (kPresent) {
                /* i. Let kValue be the result of calling the [[Get]] internal method of O with argument Pk. */
                ejsval kValue = _ejs_object_getprop (O, Pk);
                /* ii. Call the [[Call]] internal method of callbackfn with T as the this value and argument list containing kValue, k, and O. */
                foreach_args[0] = kValue;
                foreach_args[1] = NUMBER_TO_EJSVAL(k);
                foreach_args[2] = O;
                _ejs_invoke_closure (callbackfn, thisArg, 3, foreach_args);
            }
            /* d. Increase k by 1. */
            k++;
        }
    }

    return _ejs_undefined;
}

// ECMA262: 15.4.4.19
static ejsval
_ejs_Array_prototype_map (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval callbackfn = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;
    if (argc > 0) callbackfn = args[0];
    if (argc > 1) thisArg = args[1];

    /* 1. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);

    /* 2. Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".*/
    ejsval lenVal = _ejs_object_getprop (O, _ejs_atom_length);
        
    /* 3. Let len be ToUint32(lenValue). */
    uint32_t len = ToUint32(lenVal);

    /* 4. If IsCallable(callbackfn) is false, throw a TypeError exception. */
    if (!EJSVAL_IS_CALLABLE(callbackfn)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Array.prototype.map called with a non-function.");
    }

    /* 5. If thisArg was supplied, let T be thisArg; else let T be undefined. */
    ejsval T = thisArg;

    /* 6. Let A be a new array created as if by the expression new Array(len) where Array is the standard builtin constructor with that name and len is the value of len. */
    ejsval A = _ejs_array_new(len, EJS_FALSE);
    
    /* 7. Let k be 0. */
    uint32_t k = 0;
    /* 8. Repeat, while k < len */
    while (k < len) {
        /* a. Let Pk be ToString(k). */
        ejsval Pk = ToString (NUMBER_TO_EJSVAL(k));

        /* b. Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk. */
        EJSBool kPresent = OP(EJSVAL_TO_OBJECT(O), has_property)(O, Pk);

        /* c. If kPresent is true, then */
        if (kPresent) {
            /* i. Let kValue be the result of calling the [[Get]] internal method of O with argument Pk. */
            ejsval kValue = _ejs_object_getprop (O, Pk);
            /* ii. Let mappedValue be the result of calling the [[Call]] internal method of callbackfn with T as */
            /*     the this value and argument list containing kValue, k, and O. */
            ejsval map_args[3];
            map_args[0] = kValue;
            map_args[1] = NUMBER_TO_EJSVAL(k);
            map_args[2] = O;
            ejsval mappedValue = _ejs_invoke_closure (callbackfn, T, 2, map_args);

            /* iii. Call the [[DefineOwnProperty]] internal method of A with arguments Pk, Property */
            /*      Descriptor {[[Value]]: mappedValue, [[Writable]]: true, [[Enumerable]]: true, */
            /*      [[Configurable]]: true}, and false. */

            _ejs_object_setprop (A, NUMBER_TO_EJSVAL(k), mappedValue); // XXX

        }
        /* d. Increase k by 1. */
        k++;
    }
    /* 9. Return A. */
    return A;
}

// ECMA262: 15.4.4.21
static ejsval
_ejs_Array_prototype_reduce (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval callbackfn = _ejs_undefined;
    ejsval initialValue = _ejs_undefined;
    if (argc > 0) callbackfn = args[0];
    if (argc > 1) initialValue = args[1];

    if (EJSVAL_IS_NULL_OR_UNDEFINED(_this)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Array.prototype.reduce called on null or undefined.");
    }

    /* 1. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);
    EJSObject* Oobj = EJSVAL_TO_OBJECT(O);

    /* 2. Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length". */
    ejsval lenVal = _ejs_object_getprop (O, _ejs_atom_length);

    /* 3. Let len be ToUint32(lenValue). */
    uint32_t len = ToUint32(lenVal);

    /* 4. If IsCallable(callbackfn) is false, throw a TypeError exception. */
    if (!EJSVAL_IS_CALLABLE(callbackfn)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Array.prototype.reduce called with a non-function.");
    }

    /* 5. If len is 0 and initialValue is not present, throw a TypeError exception. */
    if (!len == 0 && argc < 2 /* don't use EJSVAL_IS_UNDEFINED(initialValue), as 'undefined' passed for initialValue passes */) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Reduce of empty array with no initial value");
    }
    /* 6. Let k be 0. */
    int k = 0;

    ejsval accumulator;

    /* 7. If initialValue is present, then */
    if (argc > 1 /* don't use EJSVAL_IS_UNDEFINED(initialValue), as 'undefined' passed for initialValue passes */) {
        /*    a. Set accumulator to initialValue. */
        accumulator = initialValue;
    }
    /* 8. Else, initialValue is not present */
    else {
        /*    a. Let kPresent be false. */
        EJSBool kPresent = EJS_FALSE;
        /*    b. Repeat, while kPresent is false and k < len */
        while (!kPresent && k < len) {
            /*       i. Let Pk be ToString(k). */
            ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));

            /*       ii. Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk. */
            kPresent = OP(EJSVAL_TO_OBJECT(O), has_property)(O, Pk);

            /*       iii. If kPresent is true, then */
            if (kPresent) {
                /*            1. Let accumulator be the result of calling the [[Get]] internal method of O with argument Pk. */
                accumulator = OP(EJSVAL_TO_OBJECT(O),get)(O, Pk, O);
            }
            /*       iv. Increase k by 1. */
            k++;
        }
        /*    c. If kPresent is false, throw a TypeError exception. */
        if (!kPresent)
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Reduce of empty array with no initial value");
    }
    /* 9. Repeat, while k < len */
    while (k < len) {
        /*    a. Let Pk be ToString(k). */
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));

        /*    b. Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk. */
        EJSBool kPresent = OP(EJSVAL_TO_OBJECT(O), has_property)(O, Pk);

        /*    c. If kPresent is true, then */
        if (kPresent) {
            /*       i. Let kValue be the result of calling the [[Get]] internal method of O with argument Pk. */
            ejsval kValue = OP(Oobj,get)(O, Pk, O);
            /*       ii. Let accumulator be the result of calling the [[Call]] internal method of callbackfn with  */
            /*           undefined as the this value and argument list containing accumulator, kValue, k, and O. */
            ejsval reduce_args[4] = {
                accumulator,
                kValue,
                NUMBER_TO_EJSVAL(k),
                O
            };
            accumulator = _ejs_invoke_closure (callbackfn, _ejs_undefined, 4, reduce_args);
        }
        /*    d. Increase k by 1. */
        k++;
    }

    /* 10. Return accumulator. */
    return accumulator;
}

// ECMA262: 15.4.4.22
// Array.prototype.reduceRight ( callbackfn [ , initialValue ] ) 
static ejsval
_ejs_Array_prototype_reduceRight (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    ejsval callbackfn = _ejs_undefined;
    ejsval initialValue = _ejs_undefined;
    if (argc > 0) callbackfn = args[0];
    if (argc > 1) initialValue = args[1];

    /* 1. Let O be the result of calling ToObject passing the this value as the argument.  */
    ejsval O = ToObject(_this);
    EJSObject* Oobj = EJSVAL_TO_OBJECT(O);

    /* 2. Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".  */
    ejsval lenValue = OP(Oobj,get)(O, _ejs_atom_length, O);

    /* 3. Let len be ToUint32(lenValue).  */
    uint32_t len = ToUint32(lenValue);

    /* 4. If IsCallable(callbackfn) is false, throw a TypeError exception.  */
    if (!EJSVAL_IS_CALLABLE(callbackfn))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "callback function is not a function");

    /* 5. If len is 0 and initialValue is not present, throw a TypeError exception.  */
    if (!len == 0 && argc > 1 /* don't use EJSVAL_IS_UNDEFINED(initialValue), as 'undefined' passed for initialValue passes */) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Reduce right of empty array with no initial value");
    }

    /* 6. Let k be len-1.  */
    int k = len - 1;

    ejsval accumulator;

    /* 7. If initialValue is present, then */
    if (argc > 1 /* don't use EJSVAL_IS_UNDEFINED(initialValue), as 'undefined' passed for initialValue passes */) {
        /*    a. Set accumulator to initialValue.  */
        accumulator = initialValue;
    }
    /* 8. Else, initialValue is not present  */
    else {
        /*    a. Let kPresent be false.  */
        EJSBool kPresent = EJS_FALSE;

        /*    b. Repeat, while kPresent is false and k ≥ 0  */
        while (!kPresent && k >= 0) {
            /*       i. Let Pk be ToString(k).  */
            ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));

            /*       ii. Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.  */
            kPresent = OP(EJSVAL_TO_OBJECT(O), has_property)(O, Pk);
            /*       iii. If kPresent is true, then  */
            if (kPresent) {
                /*            1. Let accumulator be the result of calling the [[Get]] internal method of O with argument Pk.  */
                accumulator = OP(Oobj,get)(O, Pk, O);
            }
            /*       iv. Decrease k by 1.  */
            k--;
        }
        /*    c. If kPresent is false, throw a TypeError exception.  */
        if (!kPresent)
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Reduce right of empty array with no initial value");
    }
    /* 9. Repeat, while k ≥ 0  */
    while (k >= 0) {
        /*    a. Let Pk be ToString(k).  */
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));

        /*    b. Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.  */
        EJSBool kPresent = OP(Oobj, has_property)(O, Pk);

        /*    c. If kPresent is true, then  */
        if (kPresent) {
            /*       i. Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.  */
            ejsval kValue = OP(Oobj,get)(O, Pk, O);

            /*       ii. Let accumulator be the result of calling the [[Call]] internal method of callbackfn with  */
            /*           undefined as the this value and argument list containing accumulator, kValue, k, and O.  */
            ejsval reduce_args[4] = {
                accumulator,
                kValue,
                NUMBER_TO_EJSVAL(k),
                O
            };
            accumulator = _ejs_invoke_closure (callbackfn, _ejs_undefined, 4, reduce_args);
        }
        /*    d. Decrease k by 1.  */
        k--;
    }
    /* 10. Return accumulator. */
    return accumulator;
}

// ECMA262: 15.4.4.12
static ejsval
_ejs_Array_prototype_splice (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    // start, deleteCount [ , item1 [ , item2 [ , … ] ] ]

    ejsval start;
    ejsval deleteCount = _ejs_undefined;

    if (argc == 0)
        return _ejs_array_new(0, EJS_FALSE);

    start = args[0];
    if (argc > 1)
        deleteCount = args[1];

    /* 1. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);
    /* 2. Let A be a new array created as if by the expression new Array() where Array is the standard built-in 
          constructor with that name. */
    ejsval A = _ejs_array_new(0, EJS_FALSE);
    /* 3. Let lenVal be the result of calling the [[Get]] internal method of O with argument "length". */
    ejsval lenVal = _ejs_object_getprop (O, _ejs_atom_length);
    /* 4. Let len be ToUint32(lenVal). */
    uint32_t len = ToUint32(lenVal);
    /* 5. Let relativeStart be ToInteger(start). */
    int32_t relativeStart = ToInteger(start);
    /* 6. If relativeStart is negative, let actualStart be max((len + relativeStart),0); else let actualStart be 
          min(relativeStart, len). */
    int32_t actualStart = relativeStart < 0 ? max((len + relativeStart),0) : min(relativeStart, len);

    if (EJSVAL_IS_UNDEFINED(deleteCount)) {
        deleteCount = NUMBER_TO_EJSVAL (len - actualStart);
    }

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
            ejsval from = NUMBER_TO_EJSVAL(k + actualDeleteCount - 1);
            /*        ii. Let to be ToString(k + itemCount - 1) */
            ejsval to = NUMBER_TO_EJSVAL(k + itemCount - 1);
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

// ECMA 262 (6) 22.1.3.6
// Array.prototype.fill (value, start = 0, end = this.length)
static ejsval
_ejs_Array_prototype_fill (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval value = _ejs_undefined;
    ejsval start = _ejs_undefined;
    ejsval end = _ejs_undefined;

    if (argc >= 1)
        value = args[0];

    if (argc >= 2)
        start = args[1];

    if (argc >= 3)
        end = args[2];

    /* 1. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);

    /* 3. Let lenVal be the result of Get(O, "length"). */
    ejsval lenVal = _ejs_object_getprop (O, _ejs_atom_length);
    /* 4. Let len be ToLength(lenVal). */
    uint32_t len = ToUint32(lenVal);

    /* 6. Let relativeStart be ToInteger(start). */
    int32_t relativeStart = ToInteger(start);

    /* 8. If relativeStart is negative, let k be max((len + relativeStart),0); else let k be min(relativeStart, len). */
    int32_t k;
    if (relativeStart < 0)
        k = max (len + relativeStart, 0);
    else
        k = min (relativeStart, len);

    /* 9. If end is undefined, let relativeEnd be len; else let relativeEnd be ToInteger(end). */
    int32_t relativeEnd;
    if (EJSVAL_IS_UNDEFINED(end))
        relativeEnd = len;
    else
        relativeEnd = ToInteger(end);

    /* 11. If relativeEnd is negative, let final be max((len + relativeEnd),0); else let final be min(relativeEnd, len). */
    int32_t final;
    if (relativeEnd < 0)
        final = max(len + relativeEnd, 0);
    else
        final = min(relativeEnd, len);

    /* 12. Repeat, while k < final */
    while (k < final) {
        /*  a. Let Pk be ToString(k). */
        ejsval Pk = NUMBER_TO_EJSVAL(k);

        /*  b. Let putStatus be the result of Put(O, Pk, value, true). */
        OP(EJSVAL_TO_OBJECT(O), put)(O, Pk, value, O, EJS_TRUE);

        /*  d. Increase k by 1. */
        k++;
    }

    /* 13. Return O. */
    return O;
}

// ECMA 262 (6): 22.1.3.7
// Array.prototype.filter ( callbackfn, thisArg = undefined )
static ejsval
_ejs_Array_prototype_filter (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval callbackfn = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc >= 1) callbackfn = args[0];
    if (argc >= 2) thisArg = args[1];

    if (EJSVAL_IS_NULL_OR_UNDEFINED(_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Array.find called on null or undefined");

    /* 1. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);
    EJSObject* Oobj = EJSVAL_TO_OBJECT(O);

    /* 3. Let lenValue be the result of Get(O, "length"). */
    ejsval lenValue = OP(Oobj,get)(O, _ejs_atom_length, O);
    /* 4. Let len be ToLength(lenValue). */
    uint32_t len = ToUint32(lenValue);

    /* 6. If IsCallable(callbackfn) is false, throw a TypeError exception. */
    if (!EJSVAL_IS_CALLABLE(callbackfn))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "callback function is not a function");

    /* 7. If thisArg was supplied, let T be thisArg; else let T be undefined. */
    ejsval T = thisArg;

    /* 8. Let A be undefined. */
    ejsval A = _ejs_undefined;

    /* 9. If O is an exotic Array object, then
          a. Let C be Get(O, "constructor").
          b. ReturnIfAbrupt(C).
          c. If IsConstructor(C) is true, then
             i. Let thisRealm be the running execution context’s Realm.
             ii. If thisRealm and the value of C’s [[Realm]] internal slot are the same value, then
                 1. Let A be the result of calling the [[Construct]] internal method of C with an argument list containing the single item 0.
    */

    /* 10. If A is undefined, then */
    if (EJSVAL_IS_UNDEFINED(A)) {
        /* a. Let A be the result of the abstract operation ArrayCreate with argument 0. */
        A = _ejs_array_new(0, EJS_FALSE);
    }

    /* 12. Let k be 0. */
    int k = 0;

    /* 13. Let to be 0. */
    int to = 0;

    /* 14. Repeat, while k < len */
    while (k < len) {
        /* a. Let Pk be ToString(k). */
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));
        /* b. Let kPresent be the result of HasProperty(O, Pk). */
        EJSBool kPresent = OP(Oobj,has_property)(O, Pk);

        /* d. If kPresent is true, then */
        if (kPresent) {
            /* i. Let kValue be the result of Get(O, Pk). */
            ejsval kValue = OP(Oobj,get)(O, Pk, O);

            /* iii. Let selected be the result of calling the [[Call]] internal method of callbackfn with T as
               thisArgument and a List containing kValue, k, and O as argumentsList. */
            ejsval argumentsList[3] = {
                kValue,
                NUMBER_TO_EJSVAL(k),
                O
            };
            ejsval selected = _ejs_invoke_closure (callbackfn, T, 3, argumentsList);

            /* v. If ToBoolean(selected) is true, then */
            if (EJSVAL_TO_BOOLEAN(ToBoolean(selected))) {
                /* 1. Let status be the result of CreateDataPropertyOrThrow (A, ToString(to), kValue). */
                
                _ejs_object_define_value_property (A, NUMBER_TO_EJSVAL(to), kValue, EJS_PROP_FLAGS_ENUMERABLE | EJS_PROP_FLAGS_CONFIGURABLE | EJS_PROP_FLAGS_WRITABLE);
                /* 3. Increase to by 1. */
                to ++;
            }
        }

        /* e. Increase k by 1. */
        k ++;
    }

    /* 15. Return A. */
    return A;
}

// ECMA 262 (6): 22.1.3.8
// Array.prototype.find ( predicate , thisArg = undefined )
static ejsval
_ejs_Array_prototype_find (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval predicate = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc >= 1) predicate = args[0];
    if (argc >= 2) thisArg = args[1];

    if (EJSVAL_IS_NULL_OR_UNDEFINED(_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Array.find called on null or undefined");

    /* 1. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);
    EJSObject* Oobj = EJSVAL_TO_OBJECT(O);

    /* 3. Let lenValue be the result of Get(O, "length"). */
    ejsval lenValue = OP(Oobj,get)(O, _ejs_atom_length, O);
    /* 4. Let len be ToLength(lenValue). */
    uint32_t len = ToUint32(lenValue);

    /* 6. If IsCallable(predicate) is false, throw a TypeError exception. */
    if (!EJSVAL_IS_CALLABLE(predicate))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "callback function is not a function");

    /* 7. If thisArg was supplied, let T be thisArg; else let T be undefined. */
    ejsval T = thisArg;

    /* 8. Let k be 0. */
    uint32_t k = 0;

    /* 9. Repeat, while k < len */
    while (k < len) {
        /* a. Let Pk be ToString(k). */
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));
        /* b. Let kPresent be the result of HasProperty(O, Pk). */
        EJSBool kPresent = OP(Oobj,has_property)(O, Pk);

        /* d. If kPresent is true, then */
        if (kPresent) {
            /* i. Let kValue be the result of Get(O, Pk). */
            ejsval kValue = OP(Oobj,get)(O, Pk, O);

            ejsval predicateargs[3] = {
                kValue,
                NUMBER_TO_EJSVAL(k),
                O
            };

            /* iii. Let testResult be the result of calling the [[Call]] internal method of predicate... */
            ejsval testResult = _ejs_invoke_closure (predicate, T, 3, predicateargs);

            /* v. If ToBoolean(testResult) is true, return kValue. */
            if (EJSVAL_TO_BOOLEAN(ToBoolean(testResult)))
                return kValue;
        }

        /* e. Increase k by 1. */
        k++;
    }

    /* 10. Return undefined. */
    return _ejs_undefined;
}

// ECMA 262 (6): 22.1.3.9
// Array.prototype.findIndex ( predicate , thisArg = undefined )
static ejsval
_ejs_Array_prototype_findIndex (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval predicate = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc >= 1) predicate = args[0];
    if (argc >= 2) thisArg = args[1];

    if (EJSVAL_IS_NULL_OR_UNDEFINED(_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Array.findIndex called on null or undefined");

    /* 1. Let O be the result of calling ToObject passing the this value as the argument. */
    ejsval O = ToObject(_this);
    EJSObject* Oobj = EJSVAL_TO_OBJECT(O);

    /* 3. Let lenValue be the result of Get(O, "length"). */
    ejsval lenValue = OP(Oobj,get)(O, _ejs_atom_length, O);
    /* 4. Let len be ToLength(lenValue). */
    uint32_t len = ToUint32(lenValue);

    /* 6. If IsCallable(predicate) is false, throw a TypeError exception. */
    if (!EJSVAL_IS_CALLABLE(predicate))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "callback function is not a function");

    /* 7. If thisArg was supplied, let T be thisArg; else let T be undefined. */
    ejsval T = thisArg;

    /* 8. Let k be 0. */
    uint32_t k = 0;

    /* 9. Repeat, while k < len */
    while (k < len) {
        /* a. Let Pk be ToString(k). */
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));
        /* b. Let kPresent be the result of HasProperty(O, Pk). */
        EJSBool kPresent = OP(Oobj,has_property)(O, Pk);

        /* d. If kPresent is true, then */
        if (kPresent) {
            /* i. Let kValue be the result of Get(O, Pk). */
            ejsval kValue = OP(Oobj,get)(O, Pk, O);

            ejsval predicateargs[3] = {
                kValue,
                NUMBER_TO_EJSVAL(k),
                O
            };

            /* iii. Let testResult be the result of calling the [[Call]] internal method of predicate... */
            ejsval testResult = _ejs_invoke_closure (predicate, T, 3, predicateargs);

            /* v. If ToBoolean(testResult) is true, return k. */
            if (EJSVAL_TO_BOOLEAN(ToBoolean(testResult)))
                return NUMBER_TO_EJSVAL(k);
        }

        /* e. Increase k by 1. */
        k++;
    }

    /* 10. Return -1. */
    return NUMBER_TO_EJSVAL(-1);
}

int
_ejs_array_indexof (EJSArray* haystack, ejsval needle)
{
    int rv = -1;
    if (EJSVAL_IS_NULL(needle)) {
        for (int i = 0; i < EJSARRAY_LEN(haystack); i ++) {
            if (EJSVAL_IS_NULL (EJSDENSEARRAY_ELEMENTS(haystack)[i])) {
                rv = i;
                break;
            }
        }
    }
    else {
        for (int i = 0; i < EJSARRAY_LEN(haystack); i ++) {
            if (EJSVAL_TO_BOOLEAN(_ejs_op_strict_eq (needle, EJSDENSEARRAY_ELEMENTS(haystack)[i]))) {
                rv = i;
                break;
            }
        }
    }

    return rv;
}

// XXX toshok - isnt' this method supposed to be generic?
static ejsval
_ejs_Array_prototype_indexOf (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    if (argc != 1)
        return NUMBER_TO_EJSVAL (-1);

    return NUMBER_TO_EJSVAL(_ejs_array_indexof((EJSArray*)EJSVAL_TO_OBJECT(_this), args[0]));
}

static ejsval
_ejs_Array_of (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    return _ejs_array_new_copy (argc, args);
}

static ejsval
_ejs_Array_from (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval itemsArg = _ejs_undefined;
    ejsval mapfn = _ejs_undefined;
    /*
    ejsval thisArg = _ejs_undefined;
    ejsval T;
    */
    EJSBool mapping;

    if (argc >= 1)
        itemsArg = args[0];

    /*
    if (argc >= 2)
        mapfn = args[1];

    if (argc >= 3)
        thisArg = args[2];
    */

    /* 1. Let C be the this value. */
    /*ejsval C = _this; */

    /* 2. Let items be ToObject(arrayLike). */
    ejsval items = ToObject(itemsArg);
    EJSObject *itemsObj = EJSVAL_TO_OBJECT(items);

    if (EJSVAL_IS_UNDEFINED(mapfn))
        /* 3. If mapfn is undefined, then let mapping be false. */
        mapping = EJS_FALSE;
    else {
        /* 5. */

        /*  a. If IsCallable(mapfn) is false, throw a TypeError exception. */
        if (!EJSVAL_IS_CALLABLE(mapfn))
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "map function is not a function");

        /*  b.If thisArg was supplied, let T be thisArg; else let T be undefined. */
        /*T = thisArg; */

        /*  c. Let mapping be true */
        mapping = EJS_TRUE;
    }

    /* 6. Let usingIterator be IsIterable(items). */
    EJSBool usingIterator = EJS_FALSE;

    /* 8. If usingIterator is true, then */
    if (usingIterator) {
    }

    /* 10. Let lenValue be the result of Get(items, "length"). */
    ejsval lenValue = _ejs_object_getprop(items, _ejs_atom_length);
    /* 11. Let len be ToLength(lenValue). */
    uint32_t len = ToUint32(lenValue);

    ejsval A = _ejs_undefined;
    /* 13. If IsConstructor(C) is true, then */
    if (EJS_FALSE) {
    } else
        /* 14. Else */
        /*  a. Let A be the result of the abstract operation ArrayCreate with argument len. */
        A = _ejs_array_new (len, EJS_FALSE);

    /* 16. Let k be 0. */
    uint32_t k = 0;

    /* 17. Repeat, while k < len */
    while (k < len) {
        /* a. Let Pk be ToString(k). */
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));
        /* b. Let kPresent be the result of HasProperty(items, Pk). */
        EJSBool kPresent = OP(itemsObj,has_property)(items, Pk);

        /* d. If kPresent is true, then */
        if (kPresent) {
            /* i. Let kValue be the result of Get(items, Pk). */
            ejsval kValue = _ejs_object_getprop(items, Pk);

            ejsval mappedValue = _ejs_undefined;

            /* iii. If mapping is true, then */
            if (mapping) {
            } else
            /* iv. Else, let mappedValue be kValue. */
                mappedValue = kValue;

            /* v. Let defineStatus be the result of CreateDataPropertyOrThrow(A, Pk, mappedValue). */
            _ejs_object_setprop (A, Pk, mappedValue);
        }

        /* e. Increase k by 1. */
        k++;
    }

    /* 18. Let putStatus be the result of Put(A, "length", len, true). */
    _ejs_object_setprop (A, _ejs_atom_length, NUMBER_TO_EJSVAL(len));

    /* 20. Return A. */
    return A;
}

static ejsval
_ejs_Array_isArray (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    if (argc == 0 || EJSVAL_IS_NULL(args[0]) || !EJSVAL_IS_OBJECT(args[0]))
        return _ejs_false;

    EJSObject* obj = EJSVAL_TO_OBJECT(args[0]);
    // spec says the following string compare on internal classname,
    // but we can just directly compare pointers.
    //return BOOLEAN_TO_EJSVAL (!strcmp (CLASSNAME(EJSVAL_TO_OBJECT(args[0])), "Array"));
    return BOOLEAN_TO_EJSVAL(obj->ops == &_ejs_Array_specops || obj->ops == &_ejs_sparsearray_specops);
}

static ejsval
_ejs_Array_create(ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    // 1. Let F be the this value. 
    ejsval F = _this;

    if (!EJSVAL_IS_CONSTRUCTOR(F)) 
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "'this' in Array[Symbol.create] is not a constructor");

    EJSObject* F_ = EJSVAL_TO_OBJECT(F);

    // 2. Let proto be GetPrototypeFromConstructor(F, "%ArrayPrototype%"). 
    // 3. ReturnIfAbrupt(proto). 
    ejsval proto = OP(F_,get)(F, _ejs_atom_prototype, F);
    if (EJSVAL_IS_UNDEFINED(proto))
        proto = _ejs_Array_prototype;

    // 4. Let obj be ArrayCreate(undefined, proto). 
    ejsval obj = _ejs_array_create(_ejs_undefined, proto);
    // 5. Return obj
    return obj;
}

ejsval
_ejs_array_create (ejsval length, ejsval proto)
{
    // EJSBool init_state = EJS_TRUE;
    if (EJSVAL_IS_UNDEFINED(length)) {
        length = NUMBER_TO_EJSVAL(0);
        //init_state = EJS_FALSE;
    }

    ejsval A = _ejs_array_new(ToUint32(length), EJS_TRUE);
    EJSObject* A_ = EJSVAL_TO_OBJECT(A);
    A_->proto = proto;
    return A;
}

void
_ejs_array_init(ejsval global)
{
    _ejs_sparsearray_specops =  _ejs_Array_specops;

    _ejs_Array = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Array, (EJSClosureFunc)_ejs_Array_impl);

    _ejs_object_setprop (global,           _ejs_atom_Array,      _ejs_Array);

    _ejs_gc_add_root (&_ejs_Array_prototype);
    _ejs_Array_prototype = _ejs_array_new(0, EJS_FALSE);
    EJSVAL_TO_OBJECT(_ejs_Array_prototype)->proto = _ejs_Object_prototype;
    _ejs_object_define_value_property (_ejs_Array, _ejs_atom_prototype, _ejs_Array_prototype, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property (_ejs_Array_prototype, _ejs_atom_constructor, _ejs_Array,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_Array, create, _ejs_Array_create, EJS_PROP_NOT_ENUMERABLE);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Array, x, _ejs_Array_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Array_prototype, x, _ejs_Array_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)

    OBJ_METHOD(isArray);
    // ECMA 6
    OBJ_METHOD(of);
    OBJ_METHOD(from);

    PROTO_METHOD(push);
    PROTO_METHOD(pop);
    PROTO_METHOD(shift);
    PROTO_METHOD(unshift);
    PROTO_METHOD(concat);
    PROTO_METHOD(slice);
    PROTO_METHOD(splice);
    PROTO_METHOD(indexOf);
    PROTO_METHOD(lastIndexOf);
    PROTO_METHOD(join);
    PROTO_METHOD(forEach);
    PROTO_METHOD(every);
    PROTO_METHOD(some);
    PROTO_METHOD(map);
    PROTO_METHOD(reduce);
    PROTO_METHOD(reduceRight);
    PROTO_METHOD(filter);
    PROTO_METHOD(reverse);
    // ECMA 6
    PROTO_METHOD(fill);
    PROTO_METHOD(find);
    PROTO_METHOD(findIndex);

    PROTO_METHOD(toString);

#undef OBJ_METHOD
#undef PROTO_METHOD
}

static ejsval
_ejs_array_specop_get (ejsval obj, ejsval propertyName, ejsval receiver)
{
    // check if propertyName is an integer, or a string that we can convert to an int
    EJSBool is_index = EJS_FALSE;
    ejsval idx_val = ToNumber(propertyName);
    int idx;
    if (EJSVAL_IS_NUMBER(idx_val)) {
        double n = EJSVAL_TO_NUMBER(idx_val);
        if (floor(n) == n) {
            idx = (int)n;
            is_index = EJS_TRUE;
        }
    }

    if (is_index) {
        if (idx < 0 || idx >= EJS_ARRAY_LEN(obj)) {
            //printf ("getprop(%d) on an array, returning undefined\n", idx);
            return _ejs_undefined;
        }
        return EJS_DENSE_ARRAY_ELEMENTS(obj)[idx];
    }

    // we also handle the length getter here
    if (EJSVAL_IS_STRING(propertyName) && !ucs2_strcmp (_ejs_ucs2_length, EJSVAL_TO_FLAT_STRING(propertyName))) {
        return NUMBER_TO_EJSVAL (EJS_ARRAY_LEN(obj));
    }

    // otherwise we fallback to the object implementation
    return _ejs_Object_specops.get (obj, propertyName, receiver);
}

static EJSPropertyDesc*
_ejs_array_specop_get_own_property (ejsval obj, ejsval propertyName)
{
    // check if propertyName is an integer, or a string that we can convert to an int
    EJSBool is_index = EJS_FALSE;
    ejsval idx_val = ToNumber(propertyName);
    int idx;
    if (EJSVAL_IS_NUMBER(idx_val)) {
        double n = EJSVAL_TO_NUMBER(idx_val);
        if (floor(n) == n) {
            idx = (int)n;
            is_index = EJS_TRUE;
        }
    }

    if (is_index) {
        if (idx >= 0 || idx < EJS_ARRAY_LEN(obj)) {
            // XXX we leak this.  need to change get_own_property to use an out param instead of a return value
            EJSPropertyDesc* desc = (EJSPropertyDesc*)calloc(sizeof(EJSPropertyDesc), 1);
            _ejs_property_desc_set_writable (desc, EJS_TRUE);
            _ejs_property_desc_set_value (desc, EJS_DENSE_ARRAY_ELEMENTS(obj)[idx]);
            return desc;
        }
    }


    if (EJSVAL_IS_STRING(propertyName) && !ucs2_strcmp (_ejs_ucs2_length, EJSVAL_TO_FLAT_STRING(propertyName))) {
        EJSArray* arr = (EJSArray*)EJSVAL_TO_OBJECT(obj);
        _ejs_property_desc_set_value (&arr->array_length_desc, NUMBER_TO_EJSVAL(EJSARRAY_LEN(arr)));
        return &arr->array_length_desc;
    }

    return _ejs_Object_specops.get_own_property (obj, propertyName);
}

static void
_ejs_array_specop_put (ejsval obj, ejsval propertyName, ejsval val, ejsval receiver, EJSBool flag)
{
    // check if propertyName is a uint32, or a string that we can convert to an uint32
    EJSBool is_index = EJS_FALSE;
    ejsval idx_val = ToNumber(propertyName);
    int idx;
    if (EJSVAL_IS_NUMBER(idx_val)) {
        double n = EJSVAL_TO_NUMBER(idx_val);
        if (floor(n) == n) {
            idx = (int)n;
            is_index = EJS_TRUE;
        }
    }

    if (is_index) {
        if (EJSVAL_IS_DENSE_ARRAY(obj)) {
            // we're a dense array, realloc to include up to idx+1

            if (idx >= EJS_DENSE_ARRAY_ALLOC(obj)) {
                // we're doing a store to an index larger than our allocated space
                // XXX this is of course totally insane, as it causes:
                //     a=[]; a[10000000]=1;
                //     to allocate 10000000+1 ejsvals.
                //     need some logic to switch to a sparse array if need be.
                maybe_realloc_dense ((EJSArray*)EJSVAL_TO_OBJECT(obj), idx - EJS_DENSE_ARRAY_ALLOC(obj) + 1);
            }

            // if we now have a hole, fill in the range with special values to indicate this
            if (idx > EJS_ARRAY_LEN(obj)) {
                for (int i = idx; i >= EJS_ARRAY_LEN(obj); i --) {
                    EJS_DENSE_ARRAY_ELEMENTS(obj)[i] = MAGIC_TO_EJSVAL_IMPL(EJS_ARRAY_HOLE);
                }
            }

            EJS_DENSE_ARRAY_ELEMENTS(obj)[idx] = val;
        }
        else {
            // we're already sparse, just give up as none of this is implemented yet.
            EJS_NOT_IMPLEMENTED();
        }
        EJS_ARRAY_LEN(obj) = MAX(EJS_ARRAY_LEN(obj), idx + 1);
        return;
    }
    // if we fail there, we fall back to the object impl below

    _ejs_Object_specops.put (obj, propertyName, val, receiver, flag);
}

static EJSBool
_ejs_array_specop_has_property (ejsval obj, ejsval propertyName)
{
    // check if propertyName is an integer, or a string that we can convert to an int
    ejsval idx_val = ToNumber(propertyName);
    int idx;
    if (EJSVAL_IS_NUMBER(idx_val)) {
        double n = EJSVAL_TO_NUMBER(idx_val);
        if (floor(n) == n) {
            idx = (int)n;
            return idx >= 0 && idx < EJS_ARRAY_LEN(obj);
        }
    }

    // if we fail there, we fall back to the object impl below

    return _ejs_Object_specops.has_property (obj, propertyName);
}

static EJSBool
_ejs_array_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag)
{
    // check if propertyName is a uint32, or a string that we can convert to an uint32
    int idx = -1;
    if (EJSVAL_IS_NUMBER(propertyName)) {
        double n = EJSVAL_TO_NUMBER(propertyName);
        if (floor(n) == n) {
            idx = (int)n;
        }
    }

    if (idx == -1)
        return _ejs_Object_specops._delete (obj, propertyName, flag);

    // if it's outside the array bounds, do nothing
    if (idx < EJS_ARRAY_LEN(obj))
        EJS_DENSE_ARRAY_ELEMENTS(obj)[idx] = _ejs_undefined;
    return EJS_TRUE;
}

static EJSBool
_ejs_array_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag)
{
    // check if propertyName is a uint32, or a string that we can convert to an uint32
    EJSBool is_index = EJS_FALSE;
    ejsval idx_val = ToNumber(propertyName);
    int idx;
    if (EJSVAL_IS_NUMBER(idx_val)) {
        double n = EJSVAL_TO_NUMBER(idx_val);
        if (floor(n) == n) {
            idx = (int)n;
            is_index = EJS_TRUE;
        }
    }

    if (is_index) {
        if (EJSVAL_IS_DENSE_ARRAY(obj)) {
            // we're a dense array, realloc to include up to idx+1

            if (idx >= EJS_DENSE_ARRAY_ALLOC(obj)) {
                // we're doing a store to an index larger than our allocated space
                // XXX this is of course totally insane, as it causes:
                //     a=[]; a[10000000]=1;
                //     to allocate 10000000+1 ejsvals.
                //     need some logic to switch to a sparse array if need be.
                maybe_realloc_dense ((EJSArray*)EJSVAL_TO_OBJECT(obj), idx - EJS_DENSE_ARRAY_ALLOC(obj) + 1);

                if (idx > EJS_ARRAY_LEN(obj)) {
                    for (int i = idx; i >= EJS_ARRAY_LEN(obj); i --) {
                        EJS_DENSE_ARRAY_ELEMENTS(obj)[i] = MAGIC_TO_EJSVAL_IMPL(EJS_ARRAY_HOLE);
                    }
                }
            }

            EJS_DENSE_ARRAY_ELEMENTS(obj)[idx] = propertyDescriptor->value;
        }
        else {
            // we're already sparse, just give up as none of this is implemented yet.
            EJS_NOT_IMPLEMENTED();
        }
        EJS_ARRAY_LEN(obj) = MAX(EJS_ARRAY_LEN(obj), idx + 1);
        return EJS_TRUE;
    }

    propertyName = ToString(propertyName);
    if (!ucs2_strcmp (_ejs_ucs2_length, EJSVAL_TO_FLAT_STRING(propertyName))) {
        // XXX more from 15.4.5.1 here
        int newLen = ToUint32(_ejs_property_desc_get_value(propertyDescriptor));
        int oldLen = EJS_ARRAY_LEN(obj);

        if (EJSVAL_IS_DENSE_ARRAY(obj)) {
            if (newLen > oldLen) {
                maybe_realloc_dense ((EJSArray*)EJSVAL_TO_OBJECT(obj), newLen);
                for (int i = oldLen; i < newLen; i ++)
                    EJS_DENSE_ARRAY_ELEMENTS(obj)[i] = MAGIC_TO_EJSVAL_IMPL(EJS_ARRAY_HOLE);
            }
        }
        else {
        }

        EJS_ARRAY_LEN(obj) = newLen;
        return EJS_TRUE;
    }

    return _ejs_Object_specops.define_own_property (obj, propertyName, propertyDescriptor, flag);
}

static EJSObject*
_ejs_array_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSArray);
}


static void
_ejs_array_specop_finalize (EJSObject* obj)
{
    if (EJSARRAY_IS_SPARSE(obj)) {
        EJSArray* arr = (EJSArray*)obj;
        for (int i = 0; i < arr->sparse.arraylet_num; i ++) {
            Arraylet al = arr->sparse.arraylets[i];
            free (al.elements);
        }
        free (arr->sparse.arraylets);
    }
    else {
        free (EJSDENSEARRAY_ELEMENTS(obj));
    }
    _ejs_Object_specops.finalize (obj);
}

static void
_ejs_array_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    if (EJSARRAY_IS_SPARSE(obj)) {
        EJSArray* arr = (EJSArray*)obj;

        for (int i = 0; i < arr->sparse.arraylet_num; i ++) {
            Arraylet al = arr->sparse.arraylets[i];
            for (int j = 0; j < al.length; j ++)
                scan_func (al.elements[j]);
        }
    }
    else {
        for (int i = 0; i < EJSARRAY_LEN(obj); i ++)
            scan_func (EJSDENSEARRAY_ELEMENTS(obj)[i]);
    }
    _ejs_Object_specops.scan (obj, scan_func);
}

EJS_DEFINE_CLASS(Array,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 _ejs_array_specop_get,
                 _ejs_array_specop_get_own_property,
                 OP_INHERIT, // get_property
                 _ejs_array_specop_put,
                 OP_INHERIT, // can_put
                 _ejs_array_specop_has_property,
                 _ejs_array_specop_delete,
                 OP_INHERIT, // default_value
                 _ejs_array_specop_define_own_property,
                 OP_INHERIT, // has_instance
                 _ejs_array_specop_allocate,
                 _ejs_array_specop_finalize,
                 _ejs_array_specop_scan
                 )

EJSSpecOps _ejs_sparsearray_specops;

