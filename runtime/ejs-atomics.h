/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_atomics_h_
#define _ejs_atomics_h_

#include "ejs-object.h"
#include "ejs-typedarrays.h"

// A SharedArrayBuffer reuses the ArrayBuffer representation (same
// specops) so the TypedArray and DataView constructors accept it as a
// viewable buffer without any changes on their side.  The struct
// appends the resizability state.
//
// Shared buffers are distinguished from ordinary ones through the
// dependent.offset word of the union: for a non-dependent buffer that
// word is dead (only alloced_buf is live, and it does not overlap
// offset), ordinary buffers are born from zeroed cells and never write
// it, and the collector's evacuation copies whole size-class cells --
// so a tag stored there survives and can never appear in an ordinary
// buffer.
typedef struct {
    EJSArrayBuffer buf;

    uint32_t max_byte_length;
    EJSBool growable;
} EJSSharedArrayBuffer;

#define EJS_SHAREDARRAYBUFFER_TAG 0x53414221 /* 'SAB!' */

#define EJSVAL_IS_SHAREDARRAYBUFFER(v)                                  \
    (EJSVAL_IS_ARRAYBUFFER(v)                                           \
     && !EJSVAL_TO_ARRAYBUFFER(v)->dependent                            \
     && (uint32_t)EJSVAL_TO_ARRAYBUFFER(v)->data.dependent.offset == EJS_SHAREDARRAYBUFFER_TAG)

EJS_BEGIN_DECLS

extern ejsval _ejs_SharedArrayBuffer;
extern ejsval _ejs_SharedArrayBuffer_prototype;
extern ejsval _ejs_Atomics;

void _ejs_atomics_init(ejsval global);

EJS_END_DECLS

#endif /* _ejs_atomics_h_ */
