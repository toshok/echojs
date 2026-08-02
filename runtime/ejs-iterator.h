/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_iterator_h_
#define _ejs_iterator_h_

#include "ejs-object.h"

// ES2025 iterator helpers: the global Iterator constructor, the helper
// methods on %Iterator.prototype%, Iterator Helper objects (the lazy
// iterators returned by map/filter/take/drop/flatMap and
// Iterator.concat), and the wrapper Iterator.from puts around
// non-conforming iterators.

typedef enum {
    EJS_ITERATOR_HELPER_MAP,
    EJS_ITERATOR_HELPER_FILTER,
    EJS_ITERATOR_HELPER_TAKE,
    EJS_ITERATOR_HELPER_DROP,
    EJS_ITERATOR_HELPER_FLATMAP,
    EJS_ITERATOR_HELPER_CHUNKS,
    EJS_ITERATOR_HELPER_WINDOWS,
    EJS_ITERATOR_HELPER_CONCAT
} EJSIteratorHelperKind;

typedef struct {
    /* object header */
    EJSObject obj;

    EJSIteratorHelperKind kind;
    EJSBool running;         // a next()/return() is on the stack (re-entrancy is a TypeError)
    EJSBool done;            // generator state completed
    EJSBool underlying_done; // outer iterator exhausted (chunks holds a final partial buffer)
    EJSBool skipped;         // drop: the initial skip already ran
    EJSBool allow_partial;   // windows: undersized == "allow-partial"
    EJSBool window_yielded;  // windows: at least one full window came out

    ejsval iterator;         // underlying iterator (undefined for concat)
    ejsval next_method;      // its cached next

    ejsval fn;               // mapper/predicate for map/filter/flatMap
    double limit;            // take/drop remaining, chunks/windows size
    double counter;          // fn call counter

    // flatMap inner iterator / concat current inner iterator
    ejsval inner;
    ejsval inner_next;

    // chunks/windows accumulation buffer, capacity = (uint32_t)limit
    ejsval* buffer;
    uint32_t buffer_count;

    // concat: item/@@iterator-method pairs, 2*concat_count values
    ejsval* concat_pairs;
    uint32_t concat_count;
    uint32_t concat_index;
} EJSIteratorHelper;

typedef struct {
    /* object header */
    EJSObject obj;

    ejsval iterator;         // wrapped iterator
    ejsval next_method;      // its cached next
} EJSWrapForValidIterator;

#define EJSVAL_IS_ITERATOR_HELPER(v) (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_IteratorHelper_specops))
#define EJSVAL_IS_WRAP_FOR_VALID_ITERATOR(v) (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_WrapForValidIterator_specops))

EJS_BEGIN_DECLS

extern ejsval _ejs_Iterator;
extern ejsval _ejs_IteratorHelper_prototype;
extern ejsval _ejs_WrapForValidIterator_prototype;

extern EJSSpecOps _ejs_IteratorHelper_specops;
extern EJSSpecOps _ejs_WrapForValidIterator_specops;

void _ejs_iterator_helpers_init(ejsval global);

EJS_END_DECLS

#endif
