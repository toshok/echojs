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
#include "ejs-number.h"

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
        if (fill) {
            for (int i = 0; i < numElements; i ++)
                rv->dense.elements[i] = MAGIC_TO_EJSVAL_IMPL(EJS_ARRAY_HOLE);
        }
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
maybe_realloc_dense (EJSArray *arr, int high_index)
{
    if (high_index >= arr->dense.array_alloc) {
        int new_alloc = high_index + 32;
        ejsval* new_elements = (ejsval*)malloc(new_alloc * sizeof(ejsval));
        memmove (new_elements, arr->dense.elements, arr->array_length * sizeof(ejsval));
        free (arr->dense.elements);
        arr->dense.elements = new_elements;
        arr->dense.array_alloc = new_alloc;
    }
}

ejsval
_ejs_array_from_iterables (int argc, ejsval* args)
{
    ejsval rv = _ejs_array_new (0, EJS_FALSE);
    for (int i = 0; i < argc; i ++) {
        ejsval iter = args[i];
        if (EJSVAL_IS_DENSE_ARRAY(iter)) {
            EJSArray *iter_arr = (EJSArray*)EJSVAL_TO_OBJECT(iter);
            _ejs_array_push_dense (rv, EJSARRAY_LEN(iter_arr), EJSDENSEARRAY_ELEMENTS(iter_arr));
        }
        else {
            // general iterator stuff
            ejsval get_iterator = Get(iter, _ejs_Symbol_iterator);
            ejsval iterator = _ejs_invoke_closure(get_iterator, iter, 0, NULL);
            ejsval iterator_next = Get(iterator, _ejs_atom_next);
            EJSBool done = EJS_FALSE;
            while (!done) {
                ejsval iterval = _ejs_invoke_closure(iterator_next, iterator, 0, NULL);
                done = ToEJSBool(Get(iterval, _ejs_atom_done));
                if (!done) {
                    ejsval value = Get(iterval, _ejs_atom_value);
                    _ejs_array_push_dense (rv, 1, &value);
                }
            }
        }
    }
    return rv;
}


uint32_t
_ejs_array_push_dense(ejsval array, int argc, ejsval *args)
{
    EJSArray *arr = (EJSArray*)EJSVAL_TO_OBJECT(array);
    maybe_realloc_dense (arr, arr->array_length + argc);
    memmove (&EJSDENSEARRAY_ELEMENTS(arr)[EJSARRAY_LEN(arr)], args, argc * sizeof(ejsval));
    EJSARRAY_LEN(arr) += argc;
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

        if (argc == 1 && EJSVAL_IS_NUMBER(args[0])) {
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
            arr->dense.array_alloc = argc + 5;
            arr->dense.elements = (ejsval*)malloc(arr->dense.array_alloc * sizeof (ejsval));

            memmove (arr->dense.elements, args, argc * sizeof(ejsval));
        }


        return _this;
    }
}

// ES6 Draft January 15, 2015
// 22.1.3.21
// Array.prototype.shift ()
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
        memmove (EJS_DENSE_ARRAY_ELEMENTS(_this), EJS_DENSE_ARRAY_ELEMENTS(_this) + 1, sizeof(ejsval) * (len-1));
        EJS_ARRAY_LEN(_this) --;
        return first;
    }

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. If len is zero, then
    if (len == 0) {
        // a. Let putStatus be Put(O, "length", 0, true).
        // b. ReturnIfAbrupt(putStatus).
        Put(O, _ejs_atom_length, NUMBER_TO_EJSVAL(0), EJS_TRUE);
        // c. Return undefined.
        return _ejs_undefined;
    }

    // 6. Let first be Get(O, "0").
    // 7. ReturnIfAbrupt(first).
    ejsval first = Get(O, _ejs_atom_0);

    // 8. Let k be 1.
    int64_t k = 1;

    // 9. Repeat, while k < len
    while (k < len) {
        // a. Let from be ToString(k).
        ejsval from = ToString(NUMBER_TO_EJSVAL(k));
        // b. Let to be ToString(k–1).
        ejsval to = ToString(NUMBER_TO_EJSVAL(k-1));

        // c. Let fromPresent be HasProperty(O, from).
        // d. ReturnIfAbrupt(fromPresent).
        EJSBool fromPresent = HasProperty(O, from);

        // e. If fromPresent is true, then
        if (fromPresent) {
            // i. Let fromVal be Get(O, from).
            // ii. ReturnIfAbrupt(fromVal).
            ejsval fromVal = Get(O, from);

            // iii. Let putStatus be Put(O, to, fromVal, true).
            // iv. ReturnIfAbrupt(putStatus).
            Put(O, to, fromVal, EJS_TRUE);
        }
        // f. Else fromPresent is false,
        else {
            // i. Let deleteStatus be DeletePropertyOrThrow(O, to).
            // ii. ReturnIfAbrupt(deleteStatus).
            DeletePropertyOrThrow(O, to);
        }
        // g. Increase k by 1.
        k++;
    }
    // 10. Let deleteStatus be DeletePropertyOrThrow(O, ToString(len–1)).
    // 11. ReturnIfAbrupt(deleteStatus).
    DeletePropertyOrThrow(O, ToString(NUMBER_TO_EJSVAL(len - 1)));

    // 12. Let putStatus be Put(O, "length", len–1, true).
    // 13. ReturnIfAbrupt(putStatus).
    Put(O, _ejs_atom_length, NUMBER_TO_EJSVAL(len-1), EJS_TRUE);
    // 14. Return first.
    return first;
}

// ES6 Draft January 15, 2015
// 22.1.3.28
// Array.prototype.unshift ( ...items )
static ejsval
_ejs_Array_prototype_unshift (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    // EJS fast path for arrays
    if (EJSVAL_IS_DENSE_ARRAY(_this)) {
        EJSArray *arr = (EJSArray*)EJSVAL_TO_OBJECT(_this);
        maybe_realloc_dense (arr, arr->array_length + argc);
        int len = EJS_ARRAY_LEN(_this);
        memmove (EJS_DENSE_ARRAY_ELEMENTS(_this) + argc, EJS_DENSE_ARRAY_ELEMENTS(_this), sizeof(ejsval) * len);
        memmove (EJS_DENSE_ARRAY_ELEMENTS(_this), args, sizeof(ejsval) * argc);
        EJS_ARRAY_LEN(_this) += argc;
        return NUMBER_TO_EJSVAL(len + argc);
    }

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 4. Let argCount be the number of actual arguments.
    int64_t argCount = argc;

    // 6. If argCount > 0, then
    if (argCount > 0) {
        // a. If len+ argCount > 253-1, throw a TypeError exception.
        if (len + argCount > EJS_MAX_SAFE_INTEGER)
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "result too large");
            
        // b. Let k be len.
        int64_t k = len;

        // c. Repeat, while k > 0,
        while (k > 0) {
            // i. Let from be ToString(k–1).
            ejsval from = ToString(NUMBER_TO_EJSVAL(k-1));

            // ii. Let to be ToString(k+argCount –1).
            ejsval to = ToString(NUMBER_TO_EJSVAL(k+argCount - 1));

            // iii. Let fromPresent be HasProperty(O, from).
            // iv. ReturnIfAbrupt(fromPresent).
            EJSBool fromPresent = HasProperty(O, from);

            // v. If fromPresent is true, then
            if (fromPresent) {
                // 1. Let fromValue be the result of Get(O, from).
                // 2. ReturnIfAbrupt(fromValue).
                ejsval fromValue = Get(O, from);

                // 3. Let putStatus be Put(O, to, fromValue, true).
                // 4. ReturnIfAbrupt(putStatus).
                Put(O, to, fromValue, EJS_TRUE);
            }
            // vi. Else fromPresent is false,
            else {
                // 1. Let deleteStatus be DeletePropertyOrThrow(O, to).
                // 2. ReturnIfAbrupt(deleteStatus).
                DeletePropertyOrThrow(O, to);
            }
            // vii. Decrease k by 1.
            k--;
        }


        // d. Let j be 0.
        int64_t j = 0;

        // e. Let items be a List whose elements are, in left to right
        //    order, the arguments that were passed to this function
        //    invocation.
        ejsval* items = args;

        // f. Repeat, while items is not empty
        for (int i = 0; i < argc; i ++) {
            // i. Remove the first element from items and let E be the value of that element.
            ejsval E = items[i];
            // ii. Let putStatus be Put(O, ToString(j), E, true).
            // iii. ReturnIfAbrupt(putStatus).
            Put(O, ToString(NUMBER_TO_EJSVAL(j)), E, EJS_TRUE);
            // iv. Increase j by 1.
            j++;
        }
    }

    ejsval new_len = NUMBER_TO_EJSVAL(len + argCount);

    // 7. Let putStatus be Put(O, "length", len+argCount, true).
    // 8. ReturnIfAbrupt(putStatus).
    Put(O, _ejs_atom_length, new_len, EJS_TRUE);

    // 9. Return len+argCount.
    return new_len;
}

// ES6 Draft January 15, 2015
// 22.1.3.20
// Array.prototype.reverse ( )
static ejsval
_ejs_Array_prototype_reverse (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. Let middle be floor(len/2). 
    int64_t middle = len / 2;

    // 6. Let lower be 0. 
    int64_t lower = 0;

    // 7. Repeat, while lower != middle 
    while (lower != middle) {
        // a. Let upper be len - lower - 1. 
        uint32_t upper = len - lower - 1;

        // b. Let upperP be ToString(upper). 
        ejsval upperP = ToString(NUMBER_TO_EJSVAL(upper));

        // c. Let lowerP be ToString(lower). 
        ejsval lowerP = ToString(NUMBER_TO_EJSVAL(lower));

        ejsval lowerValue;
        ejsval upperValue;


        // d. Let lowerExists be HasProperty(O, lowerP).
        // e. ReturnIfAbrupt(lowerExists).
        EJSBool lowerExists = HasProperty(O, lowerP);
        // f. If lowerExists is true, then
        if (lowerExists) {
            // i. Let lowerValue be Get(O, lowerP).
            // ii. ReturnIfAbrupt(lowerValue).
            lowerValue = Get(O, lowerP);
        }

        // g. Let upperExists be HasProperty(O, upperP).
        // h. ReturnIfAbrupt(upperExists).
        EJSBool upperExists = HasProperty(O, upperP);
        // i. If upperExists is true, then
        if (upperExists) {
            // i. Let upperValue be Get(O, upperP).
            // ii. ReturnIfAbrupt(upperValue).
            upperValue = Get(O, upperP);
        }

        // j. If lowerExists is true and upperExists is true, then
        if (lowerExists && upperExists) {
            // i.   Let putStatus be Put(O, lowerP, upperValue, true).
            // ii.  ReturnIfAbrupt(putStatus).
            Put(O, lowerP, upperValue, EJS_TRUE);
            // iii. Let putStatus be Put(O, upperP, lowerValue, true).
            // iv.  ReturnIfAbrupt(putStatus).
            Put(O, upperP, lowerValue, EJS_TRUE);
        }
        // k. Else if lowerExists is false and upperExists is true, then
        else if (!lowerExists && upperExists) {
            // i.   Let putStatus be Put(O, lowerP, upperValue, true).
            // ii.  ReturnIfAbrupt(putStatus).
            Put(O, lowerP, upperValue, EJS_TRUE);
            // iii. Let deleteStatus be DeletePropertyOrThrow (O, upperP).
            // iv.  ReturnIfAbrupt(deleteStatus).
            DeletePropertyOrThrow(O, upperP);
        }
        // l. Else if lowerExists is true and upperExists is false, then
        else if (lowerExists && !upperExists) {
            // Let deleteStatus be DeletePropertyOrThrow (O, lowerP).
            // ReturnIfAbrupt(deleteStatus).
            DeletePropertyOrThrow(O, lowerP);
            // Let putStatus be Put(O, upperP, lowerValue, true).
            // ReturnIfAbrupt(putStatus).
            Put(O, upperP, lowerValue, EJS_TRUE);
        }
        // m. Else both lowerExists and upperExists are false,
        else {
            // No action is required.
        }
        // n. Increase lower by 1.
        lower++;
    }
    // 8. return O.
    return O;
}

// ES6 Draft January 15, 2015
// 22.1.3.17
// Array.prototype.push (...items)
static ejsval
_ejs_Array_prototype_push (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    if (EJSVAL_IS_DENSE_ARRAY(_this)) {
        return NUMBER_TO_EJSVAL (_ejs_array_push_dense (_this, argc, args));
    }


    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. Let items be a List whose elements are, in left to right
    //    order, the arguments that were passed to this function
    //    invocation.
    ejsval* items = args;

    // 6. Let argCount be the number of elements in items.
    int64_t argCount = argc;

    // 7. If len + argCount ≥ 253-1, throw a TypeError exception.
    if (len + argCount >= EJS_MAX_SAFE_INTEGER)
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Array.prototype.push would overflow max length");
        
    // 8. Repeat, while items is not empty
    for (int i = 0; i < argc; i ++) {
        // a. Remove the first element from items and let E be the value of the element.
        ejsval E = items[i];
        // b. Let putStatus be Put(O, ToString(len), E, true).
        // c. ReturnIfAbrupt(putStatus).
        Put(O, ToString(NUMBER_TO_EJSVAL(len)), E, EJS_TRUE);
        // d. Let len be len+1.
        len ++;
    }
    // 9. Let putStatus be Put(O, "length", len, true).
    // 10. ReturnIfAbrupt(putStatus).
    Put(O, _ejs_atom_length, NUMBER_TO_EJSVAL(len), EJS_TRUE);

    // 11. Return len.
    return NUMBER_TO_EJSVAL(len);
}

// ES6 Draft January 15, 2015
// 22.1.3.16
// Array.prototype.pop ()
static ejsval
_ejs_Array_prototype_pop (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    if (EJSVAL_IS_DENSE_ARRAY(_this)) {
        return _ejs_array_pop_dense (_this);
    }

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));
    // 5. If len is zero,
    if (len == 0) {
        // a. Let putStatus be Put(O, "length", 0, true).
        // b. ReturnIfAbrupt(putStatus).
        Put(O, _ejs_atom_length, NUMBER_TO_EJSVAL(0), EJS_TRUE);
        // c. Return undefined.
        return _ejs_undefined;
    }
    // 6. Else len > 0,
    else {
        // a. Let newLen be len–1.
        int64_t newLen = len-1;
        // b. Let indx be ToString(newLen).
        ejsval indx = ToString(NUMBER_TO_EJSVAL(newLen));
        // c. Let element be Get(O, indx).
        // d. ReturnIfAbrupt(element).
        ejsval element = Get(O, indx);
    
        // e. Let deleteStatus be DeletePropertyOrThrow(O, indx).
        // f. ReturnIfAbrupt(deleteStatus).
        DeletePropertyOrThrow(O, indx);

        // g. Let putStatus be Put(O, "length", newLen, true).
        // h. ReturnIfAbrupt(putStatus).
        Put(O, _ejs_atom_length, NUMBER_TO_EJSVAL(newLen), EJS_TRUE);
        // i Return element.
        return element;
    }
}

// ES6 Draft January 15, 2015
// 22.1.3.1
// Array.prototype.concat (...arguments)
static ejsval
_ejs_Array_prototype_concat (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let A be ArraySpeciesCreate(O, 0).
    // 4. ReturnIfAbrupt(A).
    // XXX(toshok) we need ArraySpeciesCreate
    ejsval A = _ejs_array_new(0, EJS_FALSE);

    // 5. Let n be 0.
    int64_t n = 0;

    // 6. Let items be a List whose first element is O and whose
    //    subsequent elements are, in left to right order, the arguments
    //    that were passed to this function invocation.
    int num_items = argc;
    int item_idx = -1;

    // 7. Repeat, while items is not empty
    while (item_idx < num_items) {
        // a. Remove the first element from items and let E be the value of the element.
        ejsval E = item_idx == -1 ? O : args[item_idx];
        item_idx ++;

        // b. Let spreadable be IsConcatSpreadable(E).
        // c. ReturnIfAbrupt(spreadable).
        EJSBool spreadable = IsConcatSpreadable(E);

        // d. If spreadable is true, then
        if (spreadable) {
            //    i. Let k be 0.
            int64_t k = 0;
            //    ii. Let len be ToLength(Get(E, "length")).
            //    iii. ReturnIfAbrupt(len).
            int64_t len = ToLength(Get(E, _ejs_atom_length));

            //    iv. If n + len+ ≥ 253-1, throw a TypeError exception.
            if (n + len > EJS_MAX_SAFE_INTEGER)
                _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "result too large in Array.prototype.concat");

            //    v. Repeat, while k < len
            while (k < len) {
                // 1. Let P be ToString(k).
                ejsval P = ToString(NUMBER_TO_EJSVAL(k));
                // 2. Let exists be HasProperty(E, P).
                // 3. ReturnIfAbrupt(exists).
                EJSBool exists = HasProperty(E, P);
                // 4. If exists is true, then
                if (exists) {
                    // a. Let subElement be Get(E, P).
                    // b. ReturnIfAbrupt(subElement).
                    ejsval subElement = Get(E, P);
                    // c. Let status be CreateDataPropertyOrThrow (A, ToString(n), subElement).
                    // d. ReturnIfAbrupt(status).
                    _ejs_object_define_value_property (A, ToString(NUMBER_TO_EJSVAL(n)), subElement, EJS_PROP_FLAGS_ENUMERABLE | EJS_PROP_FLAGS_CONFIGURABLE | EJS_PROP_FLAGS_WRITABLE);
                }
                // 5. Increase n by 1.
                n++;
                // 6. Increase k by 1.
                k++;
            }
        }
        // e. Else E is added as a single item rather than spread,
        else {
            //    i. If n≥253-1, throw a TypeError exception.
            if (n >= EJS_MAX_SAFE_INTEGER)
                _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "result too large in Array.prototype.concat");
                
            //    ii. Let status be CreateDataPropertyOrThrow (A, ToString(n), E).
            //    iii. ReturnIfAbrupt(status).
            _ejs_object_define_value_property (A, NUMBER_TO_EJSVAL(n), E, EJS_PROP_FLAGS_ENUMERABLE | EJS_PROP_FLAGS_CONFIGURABLE | EJS_PROP_FLAGS_WRITABLE);
            //    iv. Increase n by 1.
            n++;
        }
    }
    // 8. Let putStatus be Put(A, "length", n, true).
    // 9. ReturnIfAbrupt(putStatus).
    Put(A, _ejs_atom_length, NUMBER_TO_EJSVAL(n), EJS_TRUE);

    // 10. Return A.
    return A;

#if old
    // 3. Let A be undefined.
    ejsval A = _ejs_undefined;

    // 4. If O is an exotic Array object, then
    if (EJSVAL_IS_ARRAY(O)) {
        //    a. Let C be Get(O, "constructor").
        //    b. ReturnIfAbrupt(C).
        //    c. If IsConstructor(C) is true, then
        //       i. Let thisRealm be the running execution context’s Realm.
        //       ii. If thisRealm and the value of C’s [[Realm]] internal slot are the same value, then
        //           1. Let A be the result of calling the [[Construct]] internal method of C with argument (0).
    }
    // 5. If A is undefined, then
    if (EJSVAL_IS_UNDEFINED(A)) {
        //    a. Let A be ArrayCreate(0).
        // 6. ReturnIfAbrupt(A).
        A = _ejs_array_new(0, EJS_FALSE);
    }
    // 7. Let n be 0.
    int n = 0;

    // 8. Let items be a List whose first element is O and whose subsequent elements are, in left to right order, the arguments that were passed to this function invocation.
    int num_items = argc;
    int item_idx = -1;

    // 9. Repeat, while items is not empty
    while (item_idx < num_items) {
        //    a. Remove the first element from items and let E be the value of the element.
        ejsval E = item_idx == -1 ? O : args[item_idx];
        item_idx ++;
        
        //    b. Let spreadable be IsConcatSpreadable(E).
        //    c. ReturnIfAbrupt(spreadable).
        EJSBool spreadable = IsConcatSpreadable(E);
        //    d. If spreadable is true, then
        if (spreadable) {
            //       i. Let k be 0.
            int k = 0;
            //       ii. Let lenVal be Get(E, "length").
            ejsval lenVal = Get(E, _ejs_atom_length);
            
            //       iii. Let len be ToLength(lenVal). 
            //       iv. ReturnIfAbrupt(len).
            int64_t len = ToLength(lenVal);

            //       v. Repeat, while k < len
            while (k < len) {
                //          1. Let P be ToString(k).
                ejsval P = ToString(NUMBER_TO_EJSVAL(k));
                //          2. Let exists be HasProperty(E, P).
                //          3. ReturnIfAbrupt(exists).
                EJSBool exists = HasProperty(E, P);
                //          4. If exists is true, then
                if (exists) {
                    //             a. Let subElement be Get(E, P).
                    //             b. ReturnIfAbrupt(subElement).
                    ejsval subElement = Get(E, P);
                    //             c. Let status be CreateDataPropertyOrThrow (A, ToString(n), subElement).
                    //             d. ReturnIfAbrupt(status).
                    _ejs_object_define_value_property (A, ToString(NUMBER_TO_EJSVAL(n)), subElement, EJS_PROP_FLAGS_ENUMERABLE | EJS_PROP_FLAGS_CONFIGURABLE | EJS_PROP_FLAGS_WRITABLE);
                }
                //          5. Increase n by 1.
                n++;
                //          6. Increase k by 1.
                k++;
            }
        }
        //    e. Else E is added as a single item rather than spread,
        else {
            //       i. Let status be CreateDataPropertyOrThrow (A, ToString(n), E).
            //       ii. ReturnIfAbrupt(status).
            _ejs_object_define_value_property (A, NUMBER_TO_EJSVAL(n), E, EJS_PROP_FLAGS_ENUMERABLE | EJS_PROP_FLAGS_CONFIGURABLE | EJS_PROP_FLAGS_WRITABLE);
            //       iii. Increase n by 1
            n++;
        }

    }

    // 10. Let putStatus be Put(A, "length", n, true).
    // 11. ReturnIfAbrupt(putStatus).
    Put(A, _ejs_atom_length, NUMBER_TO_EJSVAL(n), EJS_TRUE);

    // 12. Return A.
    return A;
#endif

#if old_code
    int numElements;

    numElements = EJS_ARRAY_LEN(_this); // we need to verify that we haven't been call'ed on something...
    for (int i = 0; i < argc; i ++) {
        if (IsConcatSpreadable(args[i]))
            numElements += 

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
#endif
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

// ES6 Draft January 15, 2015
// 22.1.3.22
// Array.prototype.slice(start, end)
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

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. Let relativeStart be ToInteger(start).
    // 6. ReturnIfAbrupt(relativeStart).
    int64_t relativeStart = ToInteger(start);

    // 7. If relativeStart < 0, let k be max((len + relativeStart),0); else let k be min(relativeStart, len).
    int64_t k = (relativeStart < 0) ? MAX(len + relativeStart, 0) : MIN(relativeStart, len);

    // 8. If end is undefined, let relativeEnd be len; else let relativeEnd be ToInteger(end).
    // 9. ReturnIfAbrupt(relativeEnd).
    int64_t relativeEnd = (EJSVAL_IS_UNDEFINED(end)) ? len : ToInteger(end);

    // 10. If relativeEnd < 0, let final be max((len + relativeEnd),0); else let final be min(relativeEnd, len).
    int64_t final = relativeEnd < 0 ? MAX(len + relativeEnd, 0) : MIN(relativeEnd, len);

    // 11. Let count be max(final – k, 0).
    int64_t count = MAX(final - k, 0);

    // 12. Let A be ArraySpeciesCreate(O, count).
    // 13. ReturnIfAbrupt(A).
    // XXX(toshok) missing ArraySpeciesCreate
    ejsval A = _ejs_array_new(count, EJS_FALSE);

    // 14. Let n be 0.
    int64_t n = 0;

    // 15. Repeat, while k < final
    while (k < final) {
        // a. Let Pk be ToString(k).
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));

        // b. Let kPresent be HasProperty(O, Pk)
        // c. ReturnIfAbrupt(kPresent)
        EJSBool kPresent = HasProperty(O, Pk);

        // c. If kPresent is true, then
        if (kPresent) {
            // i. Let kValue be Get(O, Pk).
            // ii. ReturnIfAbrupt(kValue).
            ejsval kValue = Get(O, Pk);

            // iii. Let status be CreateDataPropertyOrThrow(A, ToString(n), kValue ).
            // iv. ReturnIfAbrupt(status).
            _ejs_object_define_value_property (A, ToString(NUMBER_TO_EJSVAL(n)), kValue, EJS_PROP_FLAGS_ENUMERABLE | EJS_PROP_FLAGS_CONFIGURABLE | EJS_PROP_FLAGS_WRITABLE);
        }

        // d. Increase k by 1.
        k++;
        // e. Increase n by 1.
        n++;
    }
    // 16. Let putStatus be Put(A, "length", n, true).
    // 17. ReturnIfAbrupt(putStatus).
    Put(A, _ejs_atom_length, NUMBER_TO_EJSVAL(n), EJS_TRUE);

    // 18. Return A.
    return A;
}

// ES6 Draft January 15, 2015
// 22.1.3.12
// Array.prototype.join(separator)
static ejsval
_ejs_Array_prototype_join (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    ejsval separator = _ejs_undefined;

    if (argc > 0) separator = args[0];
    
    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be the result of ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. If separator is undefined, let separator be the single-element String ",".
    if (EJSVAL_IS_UNDEFINED(separator))
        separator = _ejs_string_new_utf8(",");

    // 6. Let sep be ToString(separator).
    ejsval sep = ToString(separator);

    // 7. If len is zero, return the empty String.
    if (len == 0)
        return _ejs_atom_empty;

    // 8. Let element0 be the result of Get(O, "0").
    ejsval element0 = Get(O, _ejs_atom_0);

    // 9. If element0 is undefined or null, let R be the empty String; otherwise, let R be ToString(element0).
    // 10. ReturnIfAbrupt(R).
    ejsval R = (EJSVAL_IS_UNDEFINED(element0) || EJSVAL_IS_NULL(element0)) ? _ejs_atom_empty : ToString(element0);

    // 11. Let k be 1.
    int64_t k = 1;

    // 12. Repeat, while k < len
    while (k < len) {
        // a. Let S be the String value produced by concatenating R and sep.
        ejsval S = _ejs_string_concat(R, sep);

        // b. Let element be Get(O, ToString(k)).
        ejsval element = Get(O, ToString(NUMBER_TO_EJSVAL(k)));

        // c. If element is undefined or null, let next be the empty String; otherwise, let next be ToString(element).
        // d. ReturnIfAbrupt(next).
        ejsval next = (EJSVAL_IS_UNDEFINED(element) || EJSVAL_IS_NULL(element)) ? _ejs_atom_empty : ToString(element);

        // e. Let R be a String value produced by concatenating S and next.
        R = _ejs_string_concat(S, next);

        // f. Increase k by 1.
        k ++;
    }
    // 13. Return R.
    return R;
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

// ES6 Draft January 15, 2015
// 22.1.3.14
// Array.prototype.lastIndexOf(searchElement [,fromIndex])
static ejsval
_ejs_Array_prototype_lastIndexOf (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval searchElement = _ejs_undefined;

    if (argc > 0) searchElement = args[0];

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. If len is 0, return -1.
    if (len == 0) return NUMBER_TO_EJSVAL(-1);

    // 6. If argument fromIndex was passed let n be ToInteger(fromIndex); else let n be len-1.
    // 7. ReturnIfAbrupt(n).
    int64_t n = argc > 1 ? ToInteger(args[1]) : len - 1;

    int64_t k;

    // 8. If n ≥ 0, then let k be min(n, len – 1).
    if (n >= 0)
        k = min(n, len-1);
    // 9. Else, n < 0
    else
        // a. Let k be len - abs(n).
        k = len - llabs(n);

    // 10. Repeat, while k≥ 0
    while (k >= 0) {
        ejsval kstr = ToString(NUMBER_TO_EJSVAL(k));

        // a. Let kPresent be HasProperty(O, ToString(k)).
        // b. ReturnIfAbrupt(kPresent).
        EJSBool kPresent = HasProperty(O, kstr);

        // c. If kPresent is true, then
        if (kPresent) {
            // i. Let elementK be Get(O, ToString(k)).
            // ii. ReturnIfAbrupt(elementK).
            ejsval elementK = Get(O, kstr);
            // iii. Let same be the result of performing Strict Equality Comparison searchElement === elementK.
            ejsval same = _ejs_op_strict_eq (searchElement, elementK);
            // iv. If same is true, return k.
            if (EJSVAL_TO_BOOLEAN(same)) return NUMBER_TO_EJSVAL(k);
        }
        // d. Decrease k by 1.
        k--;
    }
    // 9. Return -1.
    return NUMBER_TO_EJSVAL(-1);
}


// ES6 Draft January 15, 2015
// 22.1.3.5
// Array.prototype.every ( callbackfn [ , thisArg ] )
static ejsval
_ejs_Array_prototype_every (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval callbackfn = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc > 0) callbackfn = args[0];
    if (argc > 1) thisArg = args[1];

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. If IsCallable(callbackfn) is false, throw a TypeError exception.
    if (!IsCallable(callbackfn))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "callback function is not a function");

    // 6. If thisArg was supplied, let T be thisArg; else let T be undefined.
    ejsval T = thisArg;

    // 7. Let k be 0.
    int64_t k = 0;

    // 8. Repeat, while k < len
    while (k < len) {
        // a. Let Pk be ToString(k).
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));

        // b. Let kPresent be HasProperty(O, Pk).
        // c. ReturnIfAbrupt(kPresent).
        EJSBool kPresent = HasProperty(O, Pk);

        // d. If kPresent is true, then
        if (kPresent) {
            // i. Let kValue be Get(O, Pk).
            // ii. ReturnIfAbrupt(kValue).
            ejsval kValue = Get(O, Pk);

            // iii. Let testResult be ToBoolean(Call(callbackfn, T, «kValue, k, O»)).
            // iv. ReturnIfAbrupt(testResult).
            ejsval callbackargs[3] = {
                kValue,
                NUMBER_TO_EJSVAL(k),
                O
            };
            ejsval testResult = _ejs_invoke_closure (callbackfn, T, 3, callbackargs);

            // v. If testResult is false, return false.
            if (EJSVAL_IS_BOOLEAN(testResult) && !EJSVAL_TO_BOOLEAN(testResult))
                return _ejs_false;
        }

        // d. Increase k by 1.
        k++;
    }

    // 9. Return true.
    return _ejs_true;
}

// ES6 Draft January 15, 2015
// 22.1.3.23
// Array.prototype.some ( callbackfn [ , thisArg ] ) 
static ejsval
_ejs_Array_prototype_some (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    ejsval callbackfn = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc > 0) callbackfn = args[0];
    if (argc > 1) thisArg = args[1];

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. If IsCallable(callbackfn) is false, throw a TypeError exception.
    if (!IsCallable(callbackfn))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "callback function is not a function");

    // 6. If thisArg was supplied, let T be thisArg; else let T be undefined.
    ejsval T = thisArg;

    // 7. Let k be 0.
    int64_t k = 0;

    // 8. Repeat, while k < len
    while (k < len) {
        // a. Let Pk be ToString(k).
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));

        // b. Let kPresent be HasProperty(O, Pk).
        // c. ReturnIfAbrupt(kPresent).
        EJSBool kPresent = HasProperty(O, Pk);

        // d. If kPresent is true, then
        if (kPresent) {
            // i. Let kValue be Get(O, Pk).
            // ii. ReturnIfAbrupt(kValue).
            ejsval kValue = Get(O, Pk);

            // iii. Let testResult be ToBoolean(Call(callbackfn, T, «kValue, k, and O»)).
            // iv. ReturnIfAbrupt(testResult).
            ejsval callbackargs[3] = {
                kValue,
                NUMBER_TO_EJSVAL(k),
                O
            };
            ejsval testResult = _ejs_invoke_closure (callbackfn, T, 3, callbackargs);

            // v. If testResult is true, return true.
            if (EJSVAL_IS_BOOLEAN(testResult) && EJSVAL_TO_BOOLEAN(testResult))
                return _ejs_true;
        }
        // e. Increase k by 1.
        k++;
    }
    // 9. Return false.
    return _ejs_false;
}

// ES6 Draft January 15, 2015
// 22.1.3.10
// Array.prototype.forEach ( callbackfn [ , thisArg ] ) 
static ejsval
_ejs_Array_prototype_forEach (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    ejsval callbackfn = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc > 0) callbackfn = args[0];
    if (argc > 1) thisArg = args[1];

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. If IsCallable(callbackfn) is false, throw a TypeError exception.
    if (!IsCallable(callbackfn))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "argument is not a function");

    // 6. If thisArg was supplied, let T be thisArg; else let T be undefined.
    ejsval T = thisArg;

    ejsval foreach_args[3];
    if (EJSVAL_IS_DENSE_ARRAY(O)) {
        int i;
        foreach_args[2] = O;
        for (i = 0; i < EJS_ARRAY_LEN(_this); i ++) {
            if (EJSVAL_IS_ARRAY_HOLE_MAGIC(EJS_DENSE_ARRAY_ELEMENTS(_this)[i]))
                continue;
            foreach_args[0] = EJS_DENSE_ARRAY_ELEMENTS(_this)[i];
            foreach_args[1] = NUMBER_TO_EJSVAL(i);
            _ejs_invoke_closure (callbackfn, T, 3, foreach_args);
        }
    }
    else {
        // 7. Let k be 0.
        int64_t k = 0;

        // 8. Repeat, while k < len
        while (k < len) {
            // a. Let Pk be ToString(k).
            ejsval Pk = ToString (NUMBER_TO_EJSVAL(k));

            // b. Let kPresent be HasProperty(O, Pk).
            // c. ReturnIfAbrupt(kPresent).
            EJSBool kPresent = OP(EJSVAL_TO_OBJECT(O), HasProperty)(O, Pk);

            // d. If kPresent is true, then
            if (kPresent) {
                // i. Let kValue be Get(O, Pk).
                // ii. ReturnIfAbrupt(kValue).
                ejsval kValue = Get(O, Pk);
                // iii. Let funcResult be Call(callbackfn, T, «kValue, k, O»).
                // iv. ReturnIfAbrupt(funcResult).
                foreach_args[0] = kValue;
                foreach_args[1] = NUMBER_TO_EJSVAL(k);
                foreach_args[2] = O;
                _ejs_invoke_closure (callbackfn, T, 3, foreach_args);
            }
            // e. Increase k by 1.
            k++;
        }
    }
    // 9. Return undefined.
    return _ejs_undefined;
}

// ES6 Draft January 15, 2015
// 22.1.3.15
// Array.prototype.map ( callbackfn [ , thisArg ] ) 
static ejsval
_ejs_Array_prototype_map (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval callbackfn = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;
    if (argc > 0) callbackfn = args[0];
    if (argc > 1) thisArg = args[1];

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. If IsCallable(callbackfn) is false, throw a TypeError exception.
    if (!IsCallable(callbackfn)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Array.prototype.map called with a non-function.");
    }

    // 6. If thisArg was supplied, let T be thisArg; else let T be undefined.
    ejsval T = thisArg;

    // 7. Let A be ArraySpeciesCreate(O, len).
    // 8. ReturnIfAbrupt(A).
    // XXX(toshok) missing ArraySpeciesCreate
    ejsval A = _ejs_array_new(len, EJS_FALSE);

    // 9. Let k be 0.
    int64_t k = 0;

    // 10. Repeat, while k < len
    while (k < len) {
        // a. Let Pk be ToString(k).
        ejsval Pk = ToString (NUMBER_TO_EJSVAL(k));

        // b. Let kPresent be HasProperty(O, Pk).
        // c. ReturnIfAbrupt(kPresent).
        EJSBool kPresent = OP(EJSVAL_TO_OBJECT(O), HasProperty)(O, Pk);

        // d. If kPresent is true, then
        if (kPresent) {
            // i. Let kValue be Get(O, Pk).
            // ii. ReturnIfAbrupt(kValue).
            ejsval kValue = Get(O, Pk);

            // iii. Let mappedValue be Call(callbackfn, T, «kValue, k, O»).
            // iv. ReturnIfAbrupt(mappedValue).
            ejsval map_args[3];
            map_args[0] = kValue;
            map_args[1] = NUMBER_TO_EJSVAL(k);
            map_args[2] = O;
            ejsval mappedValue = _ejs_invoke_closure (callbackfn, T, 2, map_args);

            // v. Let status be CreateDataPropertyOrThrow (A, Pk, mappedValue).
            // vi. ReturnIfAbrupt(status).
            _ejs_object_setprop(A, Pk, mappedValue);
        }
        // e. Increase k by 1.
        k++;
    }

    // 11. Return A.
    return A;
}

// ES6 Draft January 15, 2015
// 22.1.3.18
// Array.prototype.reduce ( callbackfn [ , initialValue ] )
static ejsval
_ejs_Array_prototype_reduce (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval callbackfn = _ejs_undefined;
    ejsval initialValue = _ejs_undefined;
    if (argc > 0) callbackfn = args[0];
    if (argc > 1) initialValue = args[1];

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. If IsCallable(callbackfn) is false, throw a TypeError exception.
    if (!IsCallable(callbackfn))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Array.prototype.reduce called with a non-function.");

    // 6. If len is 0 and initialValue is not present, throw a TypeError exception.
    // (use argc here instead of EJSVAL_IS_UNDEFINED(initialValue), as 'undefined' passed for initialValue passes)
    if (!len == 0 && argc <= 1) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Reduce of empty array with no initial value");
    }

    // 7. Let k be 0.
    int64_t k = 0;

    ejsval accumulator;

    // 8. If initialValue is present, then
    if (argc > 1) {
        // a. Set accumulator to initialValue.
        accumulator = initialValue;
    }
    // 9. Else initialValue is not present,
    else {
        // a. Let kPresent be false.
        EJSBool kPresent = EJS_FALSE;
        // b. Repeat, while kPresent is false and k < len
        while (!kPresent && k < len) {
            // i. Let Pk be ToString(k).
            ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));
            // ii. Let kPresent be HasProperty(O, Pk).
            // iii. ReturnIfAbrupt(kPresent).
            kPresent = OP(EJSVAL_TO_OBJECT(O), HasProperty)(O, Pk);
            // iv. If kPresent is true, then
            if (kPresent) {
                // 1. Let accumulator be Get(O, Pk).
                // 2. ReturnIfAbrupt(accumulator).
                accumulator = Get(O, Pk);
            }
            // v. Increase k by 1.
            k++;
        }
        // c. If kPresent is false, throw a TypeError exception.
        if (!kPresent)
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Reduce of empty array with no initial value");
    }
    // 10. Repeat, while k < len
    while (k < len) {
        // a. Let Pk be ToString(k).
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));
        // b. Let kPresent be HasProperty(O, Pk).
        // c. ReturnIfAbrupt(kPresent).
        EJSBool kPresent = OP(EJSVAL_TO_OBJECT(O), HasProperty)(O, Pk);
        // d. If kPresent is true, then
        if (kPresent) {
            // i. Let kValue be Get(O, Pk).
            // ii. ReturnIfAbrupt(kValue).
            ejsval kValue = Get(O, Pk);
            // iii. Let accumulator be Call(callbackfn, undefined, «accumulator, kValue, k, O»).
            // iv. ReturnIfAbrupt(accumulator).
            ejsval reduce_args[4] = {
                accumulator,
                kValue,
                NUMBER_TO_EJSVAL(k),
                O
            };
            accumulator = _ejs_invoke_closure (callbackfn, _ejs_undefined, 4, reduce_args);
        }
        // e. Increase k by 1.
        k++;
    }
    //  11. Return accumulator.
    return accumulator;
}

// ES6 Draft January 15, 2015
// 22.1.3.19
// Array.prototype.reduceRight ( callbackfn [ , initialValue ] )
static ejsval
_ejs_Array_prototype_reduceRight (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    ejsval callbackfn = _ejs_undefined;
    ejsval initialValue = _ejs_undefined;
    if (argc > 0) callbackfn = args[0];
    if (argc > 1) initialValue = args[1];


    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. If IsCallable(callbackfn) is false, throw a TypeError exception.
    if (!IsCallable(callbackfn))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Array.prototype.reduce called with a non-function.");
        
    // 6. If len is 0 and initialValue is not present, throw a TypeError exception.
    // (use argc here instead of EJSVAL_IS_UNDEFINED(initialValue), as 'undefined' passed for initialValue passes)
    if (!len == 0 && argc <= 1) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Reduce right of empty array with no initial value");
    }
    // 7. Let k be len-1.
    int64_t k = len-1;

    ejsval accumulator;

    // 8. If initialValue is present, then
    if (argc > 1) {
        // a. Set accumulator to initialValue.
        accumulator = initialValue;
    }
    // 9. Else initialValue is not present,
    else {
        // a. Let kPresent be false.
        EJSBool kPresent = EJS_FALSE;

        // b. Repeat, while kPresent is false and k ≥ 0
        while (!kPresent && k >= 0) {
            // i. Let Pk be ToString(k).
            ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));
            // ii. Let kPresent be HasProperty(O, Pk).
            // iii. ReturnIfAbrupt(kPresent).
            kPresent = OP(EJSVAL_TO_OBJECT(O), HasProperty)(O, Pk);
            // iv. If kPresent is true, then
            if (kPresent) {
                // 1. Let accumulator be Get(O, Pk).
                // 2. ReturnIfAbrupt(accumulator).
                accumulator = Get(O, Pk);
            }
            // v. Decrease k by 1.
            k--;
        }
        // c. If kPresent is false, throw a TypeError exception.
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Reduce right of empty array with no initial value");
    }
    // 10. Repeat, while k ≥ 0
    while (k >= 0) {
        // a. Let Pk be ToString(k).
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));
        // b. Let kPresent be HasProperty(O, Pk).
        // c. ReturnIfAbrupt(kPresent).
        EJSBool kPresent = OP(EJSVAL_TO_OBJECT(O), HasProperty)(O, Pk);

        // d. If kPresent is true, then
        if (kPresent) {
            // i. Let kValue be Get(O, Pk).
            // ii. ReturnIfAbrupt(kValue).
            ejsval kValue = Get(O, Pk);

            // iii. Let accumulator be Call(callbackfn, undefined, «accumulator, kValue, k, »).
            // iv. ReturnIfAbrupt(accumulator).
            ejsval reduce_args[4] = {
                accumulator,
                kValue,
                NUMBER_TO_EJSVAL(k),
                O
            };
            accumulator = _ejs_invoke_closure (callbackfn, _ejs_undefined, 4, reduce_args);
        }
        // e. Decrease k by 1.
        k--;
    }
    // 11. Return accumulator.
    return accumulator;
}

// ES6 Draft January 15, 2015
// 22.1.3.25
// Array.prototype.splice (start, deleteCount , ...items )
static ejsval
_ejs_Array_prototype_splice (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval start = _ejs_undefined;
    ejsval deleteCount = _ejs_undefined;

    if (argc > 0) start = args[0];
    if (argc > 1) deleteCount = args[1];

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. Let relativeStart be ToInteger(start).
    // 6. ReturnIfAbrupt(relativeStart).
    int64_t relativeStart = ToInteger(start);
    
    // 7. If relativeStart < 0, let actualStart be max((len +
    //    relativeStart),0); else let actualStart be
    //    min(relativeStart, len).
    int64_t actualStart = relativeStart < 0 ? max(len + relativeStart, 0) : min(relativeStart, len);

    int64_t insertCount;
    int64_t actualDeleteCount;

    // 8. If the number of actual arguments is 0, then
    if (argc == 0) {
        // a. Let insertCount be 0.
        insertCount = 0;
        // b. Let actualDeleteCount be 0.
        actualDeleteCount = 0;
    }
    // 9. Else if the number of actual arguments is 1, then
    else if (argc == 1) {
        // a. Let insertCount be 0.
        insertCount = 0;
        // b. Let actualDeleteCount be len - actualStart
        actualDeleteCount = len - actualStart;
    }
    // 10. Else,
    else {
        // a. Let insertCount be the number of actual arguments minus 2.
        insertCount = argc - 2;
        // b. Let dc be ToInteger(deleteCount).
        // c. ReturnIfAbrupt(dc).
        int64_t dc = ToInteger(deleteCount);
        // d. Let actualDeleteCount be min(max(dc,0), len – actualStart).
        actualDeleteCount = min(max(dc, 0), len - actualStart);
    }
    // 11. If len+insertCount−actualDeleteCount > 253-1, throw a TypeError exception.
    if (len + insertCount - actualDeleteCount > EJS_MAX_SAFE_INTEGER)
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "result too large");

    // 12. Let A be ArraySpeciesCreate(O, actualDeleteCount).
    // 13. ReturnIfAbrupt(A).
    // XXX(toshok) missing ArraySpeciesCreate
    ejsval A = _ejs_array_new(actualDeleteCount, EJS_FALSE);

    // 14. Let k be 0.
    int64_t k = 0;
    // 15. Repeat, while k < actualDeleteCount
    while (k < actualDeleteCount) {
        // a. Let from be ToString(actualStart+k).
        ejsval from = ToString(NUMBER_TO_EJSVAL(actualStart+k));
        // b. Let fromPresent be HasProperty(O, from).
        // c. ReturnIfAbrupt(fromPresent).
        EJSBool fromPresent = OP(EJSVAL_TO_OBJECT(O), HasProperty)(O, from);
        // d. If fromPresent is true, then
        if (fromPresent) {
            // i. Let fromValue be Get(O, from).
            // ii. ReturnIfAbrupt(fromValue).
            ejsval fromValue = Get(O, from);
            // iii. Let status be CreateDataPropertyOrThrow(A, ToString(k), fromValue).
            // iv. ReturnIfAbrupt(status).
            _ejs_object_setprop(A, ToString(NUMBER_TO_EJSVAL(k)), fromValue);
        }
        // e. Increment k by 1.
        k++;
    }
    // 16. Let putStatus be Put(A, "length", actualDeleteCount, true).
    // 17. ReturnIfAbrupt(putStatus).
    Put(A, _ejs_atom_length, NUMBER_TO_EJSVAL(actualDeleteCount), EJS_TRUE);
    
    // 18. Let items be a List whose elements are, in left to right
    //     order, the portion of the actual argument list starting
    //     with the third argument. The list is empty if fewer than
    //     three arguments were passed.
    ejsval *items = &args[2];

    // 19. Let itemCount be the number of elements in items.
    int itemCount = argc > 2 ? argc - 2 : 0;

    // 20. If itemCount < actualDeleteCount, then
    if (itemCount < actualDeleteCount) {
        // a. Let k be actualStart.
        k = actualStart;
        // b. Repeat, while k < (len – actualDeleteCount)
        while (k < len - actualDeleteCount) {
            // i. Let from be ToString(k+actualDeleteCount).
            ejsval from = ToString(NUMBER_TO_EJSVAL(k + actualDeleteCount));
            // ii. Let to be ToString(k+itemCount).
            ejsval to = ToString(NUMBER_TO_EJSVAL(k + itemCount));

            // iii. Let fromPresent be HasProperty(O, from).
            // iv. ReturnIfAbrupt(fromPresent).
            EJSBool fromPresent = OP(EJSVAL_TO_OBJECT(O), HasProperty)(O, from);

            // v. If fromPresent is true, then
            if (fromPresent) {
                // 1. Let fromValue be Get(O, from).
                // 2. ReturnIfAbrupt(fromValue).
                ejsval fromValue = Get(O, from);
                // 3. Let putStatus be Put(O, to, fromValue, true).
                // 4. ReturnIfAbrupt(putStatus).
                Put(O, to, fromValue, EJS_TRUE);
            }
            // vi. Else fromPresent is false,
            else {
                // 1. Let deleteStatus be DeletePropertyOrThrow(O, to).
                // 2. ReturnIfAbrupt(deleteStatus).
                DeletePropertyOrThrow(O, to);
            }
            // vii. Increase k by 1.
            k++;
        }
        // c. Let k be len.
        k = len;
        // d. Repeat, while k > (len – actualDeleteCount + itemCount)
        while (k > len - actualDeleteCount + itemCount) {
            // i. Let deleteStatus be DeletePropertyOrThrow(O, ToString(k–1)).
            // ii. ReturnIfAbrupt(deleteStatus).
            DeletePropertyOrThrow(O, ToString(NUMBER_TO_EJSVAL(k-1)));
            // iii. Decrease k by 1.
            k--;
        }
    }
    // 21. Else if itemCount > actualDeleteCount, then
    else {
        // a. Let k be (len – actualDeleteCount).
        k = len - actualDeleteCount;

        // b. Repeat, while k > actualStart
        while (k > actualStart) {
            // i. Let from be ToString(k + actualDeleteCount – 1).
            ejsval from = ToString(NUMBER_TO_EJSVAL(k + actualDeleteCount - 1));
            // ii. Let to be ToString(k + itemCount – 1)
            ejsval to = ToString(NUMBER_TO_EJSVAL(k + itemCount - 1));

            // iii. Let fromPresent be HasProperty(O, from).
            // iv. ReturnIfAbrupt(fromPresent).
            EJSBool fromPresent = OP(EJSVAL_TO_OBJECT(O), HasProperty)(O, from);

            // v. If fromPresent is true, then
            if (fromPresent) {
                // 1. Let fromValue be Get(O, from).
                // 2. ReturnIfAbrupt(fromValue).
                ejsval fromValue = Get(O, from);
                // 3. Let putStatus be Put(O, to, fromValue, true).
                // 4. ReturnIfAbrupt(putStatus).
                Put(O, to, fromValue, EJS_TRUE);
            }
            // vi. Else fromPresent is false,
            else {
                // 1. Let deleteStatus be DeletePropertyOrThrow(O, to).
                // 2. ReturnIfAbrupt(deleteStatus).
                DeletePropertyOrThrow(O, to);
            }
            // vii. Decrease k by 1.
            k--;
        }
    }
    // 22. Let k be actualStart.
    k = actualStart;
    // 23. Repeat, while items is not empty
    int64_t item_i = 0;
    while (item_i < itemCount) {
        // a. Remove the first element from items and let E be the value of that element.
        ejsval E = items[item_i++];
        // b. Let putStatus be Put(O, ToString(k), E, true).
        // c. ReturnIfAbrupt(putStatus).
        Put(O, ToString(NUMBER_TO_EJSVAL(k)), E, EJS_TRUE);
        // d. Increase k by 1.
        k++;
    }
    // 24. Let putStatus be Put(O, "length", len – actualDeleteCount + itemCount, true).
    // 25. ReturnIfAbrupt(putStatus).
    Put(O, _ejs_atom_length, NUMBER_TO_EJSVAL(len - actualDeleteCount + itemCount), EJS_TRUE);
    // 26. Return A.
    return A;
}

// ES6 Draft January 15, 2015
// 22.1.3.6
// Array.prototype.fill(value[,start[,end]])
static ejsval
_ejs_Array_prototype_fill (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval value = _ejs_undefined;
    ejsval start = _ejs_undefined;
    ejsval end = _ejs_undefined;

    if (argc > 0) value = args[0];
    if (argc > 1) start = args[1];
    if (argc > 2) end = args[2];


    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. Let relativeStart be ToInteger(start).
    // 6. ReturnIfAbrupt(relativeStart).
    int64_t relativeStart = ToInteger(start);

    // 7. If relativeStart < 0, let k be max((len + relativeStart),0); else let k be min(relativeStart, len).
    int64_t k = (relativeStart < 0) ? max(len + relativeStart, 0) : min(relativeStart, len);

    // 8. If end is undefined, let relativeEnd be len; else let relativeEnd be ToInteger(end).
    // 9. ReturnIfAbrupt(relativeEnd).
    int64_t relativeEnd = EJSVAL_IS_UNDEFINED(end) ? len : ToInteger(end);

    // 10. If relativeEnd < 0, let final be max((len + relativeEnd),0); else let final be min(relativeEnd, len).
    int64_t final = relativeEnd < 0 ? max(len + relativeEnd, 0) : min(relativeEnd, len);

    // 11. Repeat, while k < final
    while (k < final) {
        // a. Let Pk be ToString(k).
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));
        // b. Let putStatus be Put(O, Pk, value, true).
        // c. ReturnIfAbrupt(putStatus).
        Put(O, Pk, value, EJS_TRUE);
        // d. Increase k by 1.
        k++;
    }
    // 12. Return O.
    return O;
}

// ES6 Draft January 15, 2015
// 22.1.3.7
// Array.prototype.filter ( callbackfn [, thisArg] )
static ejsval
_ejs_Array_prototype_filter (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval callbackfn = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc > 0) callbackfn = args[0];
    if (argc > 1) thisArg = args[1];

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. If IsCallable(callbackfn) is false, throw a TypeError exception.
    if (!IsCallable(callbackfn))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "callback function is not a function");

    // 6. If thisArg was supplied, let T be thisArg; else let T be undefined.
    ejsval T = thisArg;

    // 7. Let A be ArraySpeciesCreate(O, 0).
    // 8. ReturnIfAbrupt(A).
    // XXX(toshok) missing ArraySpeciesCreate
    ejsval A = _ejs_array_new(0, EJS_FALSE);

    // 9. Let k be 0.
    int64_t k = 0;

    // 10. Let to be 0.
    int64_t to = 0;

    // 11. Repeat, while k < len
    while (k < len) {
        // a. Let Pk be ToString(k).
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));

        // b. Let kPresent be HasProperty(O, Pk).
        // c. ReturnIfAbrupt(kPresent).
        EJSBool kPresent = HasProperty(O, Pk);

        // d. If kPresent is true, then
        if (kPresent) {
            // i. Let kValue be Get(O, Pk).
            // ii. ReturnIfAbrupt(kValue).
            ejsval kValue = Get(O, Pk);

            // iii. Let selected be ToBoolean(Call(callbackfn, T, «kValue, k, O»)).
            // iv. ReturnIfAbrupt(selected).
            ejsval argumentsList[3] = {
                kValue,
                NUMBER_TO_EJSVAL(k),
                O
            };
            ejsval selected = _ejs_invoke_closure (callbackfn, T, 3, argumentsList);

            // v. If selected is true, then
            if (EJSVAL_IS_BOOLEAN(selected) && EJSVAL_TO_BOOLEAN(selected)) {
                // 1. Let status be CreateDataPropertyOrThrow (A, ToString(to), kValue).
                // 2. ReturnIfAbrupt(status).
                _ejs_object_define_value_property (A, ToString(NUMBER_TO_EJSVAL(to)), kValue, EJS_PROP_FLAGS_ENUMERABLE | EJS_PROP_FLAGS_CONFIGURABLE | EJS_PROP_FLAGS_WRITABLE);

                // 3. Increase to by 1.
                to++;
            }
        }
        // e. Increase k by 1.
        k++;
    }
    // 12. Return A.
    return A;
}

// ES6 Draft January 15, 2015
// 22.1.3.8
// Array.prototype.find ( predicate [, thisArg] )
static ejsval
_ejs_Array_prototype_find (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval predicate = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc > 0) predicate = args[0];
    if (argc > 1) thisArg = args[1];

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. If IsCallable(predicate) is false, throw a TypeError exception.
    if (!IsCallable(predicate))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "callback function is not a function");

    // 6. If thisArg was supplied, let T be thisArg; else let T be undefined.
    ejsval T = thisArg;

    // 7. Let k be 0.
    int64_t k = 0;

    // 8. Repeat, while k < len
    while (k < len) {
        // a. Let Pk be ToString(k).
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));
        // b. Let kValue be Get(O, Pk).
        // c. ReturnIfAbrupt(kValue).
        ejsval kValue = Get(O, Pk);

        // d. Let testResult be ToBoolean(Call(predicate, T, «kValue, k, O»)).
        // e. ReturnIfAbrupt(testResult).
        ejsval predicateargs[3] = {
            kValue,
            NUMBER_TO_EJSVAL(k),
            O
        };

        ejsval testResult = ToBoolean(_ejs_invoke_closure (predicate, T, 3, predicateargs));
        
        // f. If testResult is true, return kValue.
        if (EJSVAL_TO_BOOLEAN(testResult))
            return kValue;

        // g. Increase k by 1.
        k++;
    }
    // 9. Return undefined.
    return _ejs_undefined;
}

// ES6 Draft January 15, 2015
// 22.1.3.9
// Array.prototype.findIndex ( predicate [, thisArg] )
static ejsval
_ejs_Array_prototype_findIndex (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval predicate = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc > 0) predicate = args[0];
    if (argc > 1) thisArg = args[1];

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. If IsCallable(predicate) is false, throw a TypeError exception.
    if (!IsCallable(predicate))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "callback function is not a function");

    // 6. If thisArg was supplied, let T be thisArg; else let T be undefined.
    ejsval T = thisArg;

    // 7. Let k be 0.
    int64_t k = 0;

    // 8. Repeat, while k < len
    while (k < len) {
        // a. Let Pk be ToString(k).
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));
        // b. Let kValue be Get(O, Pk).
        // c. ReturnIfAbrupt(kValue).
        ejsval kValue = Get(O, Pk);

        // d. Let testResult be ToBoolean(Call(predicate, T, «kValue, k, O»)).
        // e. ReturnIfAbrupt(testResult).
        ejsval predicateargs[3] = {
            kValue,
            NUMBER_TO_EJSVAL(k),
            O
        };

        ejsval testResult = ToBoolean(_ejs_invoke_closure (predicate, T, 3, predicateargs));
        
        // f. If testResult is true, return k.
        if (EJSVAL_TO_BOOLEAN(testResult))
            return NUMBER_TO_EJSVAL(k);

        // g. Increase k by 1.
        k++;
    }
    // 9. Return -1.
    return NUMBER_TO_EJSVAL(-1);
}

// ES6 Draft January 15, 2015
// 22.1.3.3
// Array.prototype.copyWithin (target, start [, end])
static ejsval
_ejs_Array_prototype_copyWithin (ejsval env, ejsval _this, int argc, ejsval *argv)
{
    ejsval target = _ejs_undefined;
    ejsval start = _ejs_undefined;
    ejsval end = _ejs_undefined;

    if (argc > 0) target = argv[0];
    if (argc > 1) start = argv[1];
    if (argc > 2) end = argv[2];

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. Let relativeTarget be ToInteger(target).
    // 6. ReturnIfAbrupt(relativeTarget).
    int64_t relativeTarget = ToInteger(target);

    // 7. If relativeTarget < 0, let to be max((len +
    //    relativeTarget),0); else let to be min(relativeTarget, len).
    int64_t to = relativeTarget < 0 ? max((len + relativeTarget), 0) : min(relativeTarget, len);

    // 8. Let relativeStart be ToInteger(start).
    // 9. ReturnIfAbrupt(relativeStart).
    int64_t relativeStart = ToInteger(start);

    // 10. If relativeStart < 0, let from be max((len +
    //     relativeStart),0); else let from be min(relativeStart,
    //     len).
    int64_t from = relativeStart < 0 ? max((len + relativeStart), 0) : min(relativeStart, len);

    // 11. If end is undefined, let relativeEnd be len; else let relativeEnd be ToInteger(end).
    // 12. ReturnIfAbrupt(relativeEnd).
    int64_t relativeEnd = EJSVAL_IS_UNDEFINED(end) ? len : ToInteger(end);

    // 13. If relativeEnd is negative, let final be max((len +
    //     relativeEnd),0); else let final be min(relativeEnd, len).
    int64_t final = relativeEnd < 0 ? max((len + relativeEnd), 0) : min(relativeEnd, len);

    // 14. Let count be min(final-from, len-to).
    uint32_t count = min(final - from, len - to);

    // 15. If from < to and to < from+count */
    int direction;
    if (from < to && to < from + count) {
        // a. Let direction = -1.
        direction = -1;
        // b. Let from = from + count -1.
        from = from + count - 1;
        // c. Letto=to+count-1.
        to = to + count - 1;
    }
    // 16. Else,
    else {
        // a. Let direction = 1.
        direction = 1;
    }

    // 17. Repeat, while count > 0
    while (count > 0) {
        // a. Let fromKey be ToString(from).
        ejsval fromKey = ToString(NUMBER_TO_EJSVAL(from));

        // b. Let toKey be ToString(to).
        ejsval toKey = ToString(NUMBER_TO_EJSVAL(to));

        // c. Let fromPresent be HasProperty(O, fromKey).
        // d. ReturnIfAbrupt(fromPresent). */
        EJSBool fromPresent = HasProperty(O, fromKey);

        // e. If fromPresent is true, then
        if (fromPresent) {
            // i. Let fromVal be Get(O, fromKey).
            // ii. ReturnIfAbrupt(fromVal).
            ejsval fromVal = Get(O, fromKey);

            // iii. Let putStatus be Put(O, toKey, fromVal, true).
            // iv. ReturnIfAbrupt(putStatus).
            Put(O, toKey, fromVal, EJS_TRUE);
        }
        // f. Else fromPresent is false,
        else {
            // i. Let deleteStatus be DeletePropertyOrThrow(O, toKey).
            // ii. ReturnIfAbrupt(deleteStatus).
            DeletePropertyOrThrow(O, toKey);
        }

        // g. Let from be from + direction.
        from += direction;

        // h. Let to be to + direction.
        to += to + direction;

        // i. Let count be count − 1.
        count -= 1;
    }

    // 18. Return O. 
    return O;
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


// ES6 Draft January 15, 2015
// 22.1.3.11
// Array.prototype.indexOf ( searchElement [, fromIndex ])
static ejsval
_ejs_Array_prototype_indexOf (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    ejsval searchElement = _ejs_undefined;
    ejsval fromIndex = _ejs_undefined;

    if (argc > 0) searchElement = args[0];
    if (argc > 1) fromIndex = args[1];

    // 1. Let O be the result of calling ToObject passing the this value as the argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Let len be ToLength(Get(O, "length")).
    // 4. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(O, _ejs_atom_length));

    // 5. If len is 0, return −1.
    if (len == 0) return NUMBER_TO_EJSVAL(-1);

    // 6. If argument fromIndex was passed let n be ToInteger(fromIndex); else let n be 0.
    // 7. ReturnIfAbrupt(n).
    int64_t n = (argc > 1) ? ToInteger(fromIndex) : 0;

    // 8. If n ≥ len, return −1.
    if (n >= len) return NUMBER_TO_EJSVAL(-1);

    // 9. If n ≥ 0,then
    int64_t k;
    if (n >= 0)
        // a. Let k be n.
        k = n;
    // 10. Else n<0,
    else
        // a. Let k be len - abs(n).
        k = len - llabs(n);

    // b. If k<0, let k be 0.
    if (k < 0) k = 0;

    // 11. Repeat, while k<len
    while (k < len) {
        // a. Let kPresent be HasProperty(O, ToString(k)).
        // b. ReturnIfAbrupt(kPresent).
        EJSBool kPresent = HasProperty(O, ToString(NUMBER_TO_EJSVAL(k)));
        // c. If kPresent is true, then
        if (kPresent) {
            // i. Let elementK be the result of Get(O, ToString(k)).
            // ii. ReturnIfAbrupt(elementK).
            ejsval elementK = Get(O, ToString(NUMBER_TO_EJSVAL(k)));
            // iii. Let same be the result of performing Strict Equality Comparison searchElement === elementK.
            ejsval same = _ejs_op_strict_eq (searchElement, elementK);
            // iv. If same is true, return k.
            if (EJSVAL_TO_BOOLEAN(same))
                return NUMBER_TO_EJSVAL(k);
        }
        // d. Increase k by 1.
        k++;
    }
    // 12. Return -1.
    return NUMBER_TO_EJSVAL(-1);
}

// ES6 Draft January 15, 2015
// 22.1.3.13
// Array.prototype.keys ()
static ejsval
_ejs_Array_prototype_keys (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let O be the result of calling ToObject with the this value as its argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Return CreateArrayIterator(O, "key").
    return _ejs_array_iterator_new (O, EJS_ARRAYITER_KIND_KEY);
}

// ES6 Draft January 15, 2015
// 22.1.3.29
// Array.prototype.values ()
static ejsval
_ejs_Array_prototype_values (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let O be the result of calling ToObject with the this value as its argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Return CreateArrayIterator(O, "value").
    return _ejs_array_iterator_new (O, EJS_ARRAYITER_KIND_VALUE);
}

// ES6 Draft January 15, 2015
// 22.1.3.4
// Array.prototype.entries ()
static ejsval
_ejs_Array_prototype_entries (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let O be the result of calling ToObject with the this value as its argument.
    // 2. ReturnIfAbrupt(O).
    ejsval O = ToObject(_this);

    // 3. Return CreateArrayIterator(O, "key+value").
    return _ejs_array_iterator_new (O, EJS_ARRAYITER_KIND_KEYVALUE);
}

// ES6 Draft January 15, 2015
// 22.1.2.3
// Array.of (...items)
static ejsval
_ejs_Array_of (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // fast(er) path for arrays
    if (EJSVAL_IS_ARRAY(_this))
        return _ejs_array_new_copy (argc, args);

    // 1. Let len be the actual number of arguments passed to this function.
    int64_t len = argc;

    // 2. Let items be the List of arguments passed to this function.
    ejsval *items = args;

    // 3. Let C be the this value.
    ejsval C = _this;

    ejsval A;
    // 4. If IsConstructor(C) is true, then
    if (IsConstructor(C)) {
        // a. Let A be Construct(C, «len»).
        EJS_NOT_IMPLEMENTED();
    }
    // 5. Else,
    else {
        // a. Let A be ArrayCreate(len).
        A = _ejs_array_new(len, EJS_FALSE);
    }
    // 6. ReturnIfAbrupt(A).
    // 7. Let k be 0.
    int64_t k = 0;

    // 8. Repeat, while k < len
    while (k < len) {
        // a. Let kValue be element k of items.
        ejsval kValue = items[k];
        // b. Let Pk be ToString(k).
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));
        // c. Let defineStatus be CreateDataPropertyOrThrow(A,Pk, kValue.[[value]]).
        // d. ReturnIfAbrupt(defineStatus).
        _ejs_object_define_value_property (A, Pk, kValue, EJS_PROP_FLAGS_ENUMERABLE | EJS_PROP_FLAGS_CONFIGURABLE | EJS_PROP_FLAGS_WRITABLE);
        
        // e. Increase k by 1.
        k++;
    }
    // 9. Let putStatus be Put(A, "length", len, true).
    // 10. ReturnIfAbrupt(putStatus).
    Put(A, _ejs_atom_length, NUMBER_TO_EJSVAL(len), EJS_TRUE);
    // 11. Return A.
    return A;
}

ejsval
_ejs_array_from_iterables (int argc, ejsval* args);

// ES6 Draft January 15, 2015
// 22.1.2.1
// Array.from(items[,mapfn[,thisArg]])
static ejsval
_ejs_Array_from (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval items = _ejs_undefined;
    ejsval mapfn = _ejs_undefined;
    ejsval thisArg = _ejs_undefined;

    if (argc > 0) items = args[0];
    if (argc > 1) items = args[1];
    if (argc > 2) items = args[2];

    // 1. Let C be the this value.
    ejsval C = _this;

    ejsval T;

    EJSBool mapping;
    // 2. If mapfn is undefined, let mapping be false.
    if (EJSVAL_IS_UNDEFINED(mapfn)) {
        mapping = EJS_FALSE;
    }
    // 3. else
    else {
        // a. If IsCallable(mapfn) is false, throw a TypeError exception.
        if (!IsCallable(mapfn))
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "map function is not a function");

        // b. If thisArg was supplied, let T be thisArg; else let T be undefined.
        T = thisArg;
        // c. Let mapping be true
        mapping = EJS_TRUE;
    }
    // 4. Let usingIterator be GetMethod(items, @@iterator).
    // 5. ReturnIfAbrupt(usingIterator).
    ejsval usingIterator = GetMethod(items, _ejs_Symbol_iterator);

    // 6. If usingIterator is not undefined, then
    if (!EJSVAL_IS_UNDEFINED(usingIterator)) {
        EJS_NOT_IMPLEMENTED();
        // a. If IsConstructor(C) is true, then
        // i. Let A be Construct(C).
        // b. Else,
        // i. Let A be ArrayCreate(0).
        // c. ReturnIfAbrupt(A).
        // d. Let iterator be GetIterator(items, usingIterator).
        // e. ReturnIfAbrupt(iterator).
        // f. Letkbe0.
        // g. Repeat
        // i. Let Pk be ToString(k).
        // ii. Let next be IteratorStep(iterator).
        // iii. ReturnIfAbrupt(next).
        // iv. If next is false, then
        // 1. Let putStatus be Put(A, "length", k, true).
        // 2. ReturnIfAbrupt(putStatus).
        // 3. Return A.
        // v. Let nextValue be IteratorValue(next).
        // vi. ReturnIfAbrupt(nextValue).
        // vii. If mapping is true, then
        // 1. Let mappedValue be Call(mapfn, T, «nextValue, k»).
        // 2. ReturnIfAbrupt(mappedValue).
        // viii.Else, let mappedValue be nextValue.
        // ix. Let defineStatus be CreateDataPropertyOrThrow(A, Pk, mappedValue).
        // x. ReturnIfAbrupt(defineStatus).
        // xi. Increase k by 1.
    }
    // 7. Assert: items is not an Iterable so assume it is an array-like object.
    // 8. Let arrayLike be ToObject(items).
    // 9. ReturnIfAbrupt(arrayLike).
    ejsval arrayLike = ToObject(items);
    // 10. Let len be ToLength(Get(arrayLike, "length")).
    // 11. ReturnIfAbrupt(len).
    int64_t len = ToLength(Get(arrayLike, _ejs_atom_length));

    ejsval A;
    // 12. If IsConstructor(C) is true, then
    if (EJS_FALSE/*XXX(toshok)*/ && IsConstructor(C)) {
        // a. Let A be Construct(C, «len»).
        EJS_NOT_IMPLEMENTED();
    }
    // 13. Else,
    else {
        // a. Let A be ArrayCreate(len).
        A = _ejs_array_new(len, EJS_FALSE);
    }
    // 14. ReturnIfAbrupt(A).
    // 15. Let k be 0.
    int64_t k = 0;
    // 16. Repeat, while k < len
    while (k < len) {
        // a. Let Pk be ToString(k).
        ejsval Pk = ToString(NUMBER_TO_EJSVAL(k));

        // b. Let kValue be Get(arrayLike, Pk).
        // c. ReturnIfAbrupt(kValue).
        ejsval kValue = Get(arrayLike, Pk);

        ejsval mappedValue;
        // d. If mapping is true, then
        if (mapping) {
            // i. Let mappedValue be Call(mapfn, T, «kValue, k»).
            // ii. ReturnIfAbrupt(mappedValue).
            ejsval mapfnArgs[3] = {
                kValue,
                NUMBER_TO_EJSVAL(k)
            };
            mappedValue = _ejs_invoke_closure (mapfn, T, 2, mapfnArgs);
        }
        // e. Else, let mappedValue be kValue.
        else {
            mappedValue = kValue;
        }
        // f. Let defineStatus be CreateDataPropertyOrThrow(A, Pk, mappedValue).
        // g. ReturnIfAbrupt(defineStatus).
        _ejs_object_define_value_property (A, Pk, mappedValue, EJS_PROP_FLAGS_ENUMERABLE | EJS_PROP_FLAGS_CONFIGURABLE | EJS_PROP_FLAGS_WRITABLE);
        // h. Increase k by 1.
        k++;
    }
    
    // 17. Let putStatus be Put(A, "length", len, true).
    // 18. ReturnIfAbrupt(putStatus).
    Put(A, _ejs_atom_length, NUMBER_TO_EJSVAL(len), EJS_TRUE);
    // 19. Return A.
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
    return BOOLEAN_TO_EJSVAL(obj->ops == &_ejs_Array_specops || obj->ops == &_ejs_sparsearray_specops);
}

static ejsval
_ejs_Array_get_species (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    return _ejs_Array;
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
    ejsval proto = OP(F_,Get)(F, _ejs_atom_prototype, F);
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

ejsval
_ejs_array_iterator_new(ejsval array, EJSArrayIteratorKind kind)
{
    /* 1. Let O be ToObject(array). */
    /* 2. ReturnIfAbrupt(O). */
    ejsval O = ToObject(array);

    /* 3. Let iterator be ObjectCreate(%ArrayIteratorPrototype%,
     * ([[IteratedObject]], [[ArrayIteratorNextIndex]], [[ArrayIterationKind]])). */
    EJSArrayIterator *iter = _ejs_gc_new (EJSArrayIterator);
    _ejs_init_object ((EJSObject*)iter, _ejs_ArrayIterator_prototype, &_ejs_ArrayIterator_specops);

    /* 4. Set iterator’s [[IteratedObject]] internal slot to O. */
    iter->iterated = O;

    /* 5. Set iterator’s [[ArrayIteratorNextIndex]] internal slot to 0. */
    iter->next_index = 0;

    /* 6. Set iterator’s [[ArrayIterationKind]] internal slot to kind. */
    iter->kind = kind;

    /* 7. Return iterator. */
    return OBJECT_TO_EJSVAL(iter);
}

ejsval _ejs_ArrayIterator_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_ArrayIterator EJSVAL_ALIGNMENT;

static ejsval
_ejs_ArrayIterator_impl (ejsval env, ejsval _this, uint32_t argc, ejsval*args)
{
    /* Do nothing for now - as we don't allow the user to create iterators directly. */
    return _this;
}

static ejsval
_ejs_ArrayIterator_prototype_next (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval result;

    /* 1. Let O be the this value. */
    /* 2. If Type(O) is not Object, throw a TypeError exception. */
    ejsval O = _this;
    if (!EJSVAL_IS_OBJECT(O))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, ".next called on non-object");

    EJSArrayIterator *OObj = (EJSArrayIterator*)EJSVAL_TO_OBJECT(O);

    /* 3. If O does not have all of the internal slots of an Array Iterator Instance (22.1.5.3),
     * throw a TypeError exception. */
    if (!EJSVAL_IS_ARRAYITERATOR(O))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, ".next called on non-ArrayIterator instance");

    /* 4. Let a be the value of the [[IteratedObject]] internal slot of O. */
    ejsval a = OObj->iterated;

    /* 5. If a is undefined, then return CreateIterResultObject(undefined, true). */
    if (EJSVAL_IS_UNDEFINED(a))
        return _ejs_create_iter_result (_ejs_undefined, _ejs_true);

    /* 6. Let index be the value of the [[ArrayIteratorNextIndex]] internal slot of O. */
    uint32_t index = OObj->next_index;

    /* 7. Let itemKind be the value of the [[ArrayIterationKind]] internal slot of O. */
    EJSArrayIteratorKind itemKind = OObj->kind;

    /* 8. Let lenValue be Get(a, "length"). */
    ejsval lenValue = Get (a, _ejs_atom_length);

    /* 9. Let len be ToLength(lenValue). */
    int len = ToLength(lenValue);

    /* 11. If index ≥ len, then */
    if (index >= len) {
        /* a. Set the value of the [[IteratedObject]] internal slot of O to undefined. */
        OObj->iterated = _ejs_undefined;

        /* b. Return CreateIterResultObject(undefined, true). */
        return _ejs_create_iter_result (_ejs_undefined, _ejs_true);
    }

    /* 12.  Set the value of the [[ArrayIteratorNextIndex]] internal slot of O to index+1. */
    OObj->next_index = index + 1;

    /* 13. If itemKind is "key", then let result be index. */
    if (itemKind == EJS_ARRAYITER_KIND_KEY)
        result = NUMBER_TO_EJSVAL(index);
    /* 14.  Else */
    else {
        /* a. Let elementKey be ToString(index). */
        /* b. Let elementValue be Get(a, elementKey). */
        ejsval elementKey = ToString(NUMBER_TO_EJSVAL((index)));
        ejsval elementValue = Get (a, elementKey);

        /* 15. If itemKind is "value", then let result be elementValue. */
        if (itemKind == EJS_ARRAYITER_KIND_VALUE)
            result = elementValue;
        /* 16. Else, */
        else {
            /* a. Assert itemKind is "key+value" */
            /* b. Let result be ArrayCreate(2). */
            result = _ejs_array_new(2, EJS_FALSE);

            /* c. Assert: result is a new, well-formed Array object so the following operations will never fail. */
            /* d. Call CreateDataProperty(result, "0", index). */
            _ejs_object_setprop (result, NUMBER_TO_EJSVAL(0), NUMBER_TO_EJSVAL(index));

            /* e. Call CreateDataProperty(result, "1", elementValue). */
            _ejs_object_setprop (result, NUMBER_TO_EJSVAL(1), elementValue);
        }
    }

    /* 17. Return CreateIterResultObject(result, false). */
    return _ejs_create_iter_result (result, _ejs_false);
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
    EJS_INSTALL_SYMBOL_GETTER(_ejs_Array, species, _ejs_Array_get_species);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Array, x, _ejs_Array_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Array_prototype, x, _ejs_Array_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)
#define PROTO_METHOD_LEN(x,l) EJS_INSTALL_ATOM_FUNCTION_LEN_FLAGS (_ejs_Array_prototype, x, _ejs_Array_prototype_##x, l, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)

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
    PROTO_METHOD(copyWithin);
    PROTO_METHOD(fill);
    PROTO_METHOD_LEN(find, 1);
    PROTO_METHOD(findIndex);
    PROTO_METHOD(keys);

    // we expand PROTO_METHOD(values) here so that we can install the function as @@iterator below
    ejsval _values = _ejs_function_new_native (_ejs_null, _ejs_atom_values, (EJSClosureFunc)_ejs_Array_prototype_values);
    _ejs_object_define_value_property (_ejs_Array_prototype, _ejs_atom_values, _values, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);

    PROTO_METHOD(entries);

    PROTO_METHOD(toString);

#undef OBJ_METHOD
#undef PROTO_METHOD

    // 22.1.3.30 Array.prototype [ @@iterator ] ( )
    // The initial value of the @@iterator property is the same function object as the initial value of the Array.prototype.values property.
    _ejs_object_define_value_property (_ejs_Array_prototype, _ejs_Symbol_iterator, _values, EJS_PROP_NOT_ENUMERABLE);

    _ejs_ArrayIterator = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Array, (EJSClosureFunc)_ejs_ArrayIterator_impl);

    _ejs_gc_add_root (&_ejs_ArrayIterator_prototype);
    _ejs_ArrayIterator_prototype = _ejs_array_iterator_new(_ejs_Array_prototype, EJS_ARRAYITER_KIND_VALUE);
    EJSVAL_TO_OBJECT(_ejs_ArrayIterator_prototype)->proto = _ejs_Object_prototype;
    _ejs_object_define_value_property (_ejs_ArrayIterator, _ejs_atom_prototype, _ejs_ArrayIterator_prototype,
                                        EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property (_ejs_ArrayIterator_prototype, _ejs_atom_constructor, _ejs_ArrayIterator,
                                        EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

#define PROTO_ITER_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_ArrayIterator_prototype, x, _ejs_ArrayIterator_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)
    PROTO_ITER_METHOD(next);
#undef PROTO_ITER_METHOD
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
        ejsval rv = EJS_DENSE_ARRAY_ELEMENTS(obj)[idx];
        if (EJSVAL_IS_ARRAY_HOLE_MAGIC(rv))
            return _ejs_undefined;
        return rv;
    }

    // we also handle the length getter here
    if (EJSVAL_IS_STRING(propertyName) && !ucs2_strcmp (_ejs_ucs2_length, EJSVAL_TO_FLAT_STRING(propertyName))) {
        return NUMBER_TO_EJSVAL (EJS_ARRAY_LEN(obj));
    }

    // otherwise we fallback to the object implementation
    return _ejs_Object_specops.Get (obj, propertyName, receiver);
}

static EJSPropertyDesc*
_ejs_array_specop_get_own_property (ejsval obj, ejsval propertyName, ejsval *exc)
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

    return _ejs_Object_specops.GetOwnProperty (obj, propertyName, exc);
}

static EJSBool
_ejs_array_specop_set (ejsval obj, ejsval propertyName, ejsval val, ejsval receiver)
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

            // XXX this is of course totally insane, as it causes:
            //     a=[]; a[10000000]=1;
            //     to allocate 10000000+1 ejsvals.
            //     need some logic to switch to a sparse array if need be.
            maybe_realloc_dense ((EJSArray*)EJSVAL_TO_OBJECT(obj), idx);

            // if we now have a hole, fill in the range with special values to indicate this
            if (idx > EJS_ARRAY_LEN(obj)) {
                for (int i = idx-1; i >= EJS_ARRAY_LEN(obj); i --) {
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
        return EJS_TRUE;
    }
    // if we fail there, we fall back to the object impl below

    return _ejs_Object_specops.Set (obj, propertyName, val, receiver);
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
            if (idx >= 0 && idx < EJS_ARRAY_LEN(obj)) {
                ejsval element = EJS_DENSE_ARRAY_ELEMENTS(obj)[idx];
                if (EJSVAL_IS_ARRAY_HOLE_MAGIC(element))
                    return EJS_FALSE;
                return EJS_TRUE;
            }
        }
    }

    // if we fail there, we fall back to the object impl below

    return _ejs_Object_specops.HasProperty (obj, propertyName);
}

static EJSBool
_ejs_array_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag)
{
    // check if propertyName is a uint32, or a string that we can convert to an uint32
    int idx = -1;
    ejsval idx_val = ToNumber(propertyName);
    if (EJSVAL_IS_NUMBER(idx_val)) {
        double n = EJSVAL_TO_NUMBER(idx_val);
        if (floor(n) == n) {
            idx = (int)n;
        }
    }

    if (idx < 0)
        return _ejs_Object_specops.Delete (obj, propertyName, flag);

    // if it's outside the array bounds, do nothing
    if (idx < EJS_ARRAY_LEN(obj))
        EJS_DENSE_ARRAY_ELEMENTS(obj)[idx] = MAGIC_TO_EJSVAL_IMPL(EJS_ARRAY_HOLE);
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
            if (idx >= 0)
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
                maybe_realloc_dense ((EJSArray*)EJSVAL_TO_OBJECT(obj), idx);

            }

            // if we now have a hole, fill in the range with special values to indicate this
            if (idx > EJS_ARRAY_LEN(obj)) {
                for (int i = idx; i >= EJS_ARRAY_LEN(obj); i --) {
                    EJS_DENSE_ARRAY_ELEMENTS(obj)[i] = MAGIC_TO_EJSVAL_IMPL(EJS_ARRAY_HOLE);
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

    if (EJSVAL_IS_STRING(propertyName)) {
        if (!ucs2_strcmp (_ejs_ucs2_length, EJSVAL_TO_FLAT_STRING(propertyName))) {
            // XXX more from 15.4.5.1 here
            int newLen = ToUint32(_ejs_property_desc_get_value(propertyDescriptor));
            int oldLen = EJS_ARRAY_LEN(obj);

            if (EJSVAL_IS_DENSE_ARRAY(obj)) {
                if (newLen > EJS_DENSE_ARRAY_ALLOC(obj))
                    maybe_realloc_dense ((EJSArray*)EJSVAL_TO_OBJECT(obj), newLen);

                if (newLen > oldLen) {
                    for (int i = oldLen; i < newLen; i ++)
                        EJS_DENSE_ARRAY_ELEMENTS(obj)[i] = MAGIC_TO_EJSVAL_IMPL(EJS_ARRAY_HOLE);
                }
            }
            else {
                // we're already sparse, just give up as none of this is implemented yet.
                EJS_NOT_IMPLEMENTED();
            }

            EJS_ARRAY_LEN(obj) = newLen;
            return EJS_TRUE;
        }
    }

    return _ejs_Object_specops.DefineOwnProperty (obj, propertyName, propertyDescriptor, flag);
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
    _ejs_Object_specops.Finalize (obj);
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
    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(Array,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT, // [[IsExtensible]]
                 OP_INHERIT, // [[PreventExtensions]]
                 _ejs_array_specop_get_own_property,
                 _ejs_array_specop_define_own_property,
                 _ejs_array_specop_has_property,
                 _ejs_array_specop_get,
                 _ejs_array_specop_set,
                 _ejs_array_specop_delete,
                 OP_INHERIT, // [[Enumerate]]
                 OP_INHERIT, // [[OwnPropertyKeys]]
                 _ejs_array_specop_allocate,
                 _ejs_array_specop_finalize,
                 _ejs_array_specop_scan
                 )

EJSSpecOps _ejs_sparsearray_specops;

static void
_ejs_array_iterator_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSArrayIterator* iter = (EJSArrayIterator*)obj;
    scan_func(iter->iterated);
    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(ArrayIterator,
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
                 _ejs_array_iterator_specop_scan
                 )
