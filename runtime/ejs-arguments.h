/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_arguments_h_
#define _ejs_arguments_h_

#include "ejs-object.h"

#define EJSVAL_IS_ARGUMENTS(v)     (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_Arguments_specops))
#define EJSVAL_TO_ARGUMENTS(v)     ((EJSArguments*)EJSVAL_TO_OBJECT(v))

#define EJS_ARGUMENTS_HAS_OOL_BUFFER_MASK 0x10
#define EJS_ARGUMENTS_HAS_OOL_BUFFER_MASK_SHIFTED (EJS_ARGUMENTS_HAS_OOL_BUFFER_MASK << EJS_GC_USER_FLAGS_SHIFT)
#define EJS_ARGUMENTS_HAS_OOL_BUFFER(s) ((((EJSPrimString*)(s))->gc_header & EJS_ARGUMENTS_HAS_OOL_BUFFER_MASK_SHIFTED) >> EJS_GC_USER_FLAGS_SHIFT) != 0
#define EJS_ARGUMENTS_SET_HAS_OOL_BUFFER(s) ((((EJSPrimString*)(s))->gc_header |= EJS_ARGUMENTS_HAS_OOL_BUFFER_MASK_SHIFTED))

// set once [[DefineOwnProperty]] or [[Delete]] touches the object: the
// property map is always authoritative, but while this flag is clear
// the index properties are known to be plain writable data properties
// kept in sync with the side buffer, so [[Get]]/[[HasProperty]] may
// answer from the buffer without a map lookup
#define EJS_ARGUMENTS_OVERRIDDEN_MASK 0x20
#define EJS_ARGUMENTS_OVERRIDDEN_MASK_SHIFTED (EJS_ARGUMENTS_OVERRIDDEN_MASK << EJS_GC_USER_FLAGS_SHIFT)
#define EJS_ARGUMENTS_IS_OVERRIDDEN(s) (((((EJSPrimString*)(s))->gc_header & EJS_ARGUMENTS_OVERRIDDEN_MASK_SHIFTED) >> EJS_GC_USER_FLAGS_SHIFT) != 0)
#define EJS_ARGUMENTS_SET_OVERRIDDEN(s) ((((EJSPrimString*)(s))->gc_header |= EJS_ARGUMENTS_OVERRIDDEN_MASK_SHIFTED))
#define EJS_ARGUMENTS_CLEAR_OVERRIDDEN(s) ((((EJSPrimString*)(s))->gc_header &= ~EJS_ARGUMENTS_OVERRIDDEN_MASK_SHIFTED))

typedef struct {
    /* object header */
    EJSObject obj;

    /* arguments specific data */
    uint32_t argc;
    ejsval*  args;
} EJSArguments;

EJS_BEGIN_DECLS

extern ejsval _ejs_Arguments__proto__;
extern EJSSpecOps _ejs_Arguments_specops;

void   _ejs_arguments_init(ejsval global);
ejsval _ejs_arguments_new (int numElements, ejsval* args);
ejsval _ejs_arg_length (uint32_t argc, uint32_t index);

EJS_END_DECLS

#endif /* _ejs_arguments_h */
