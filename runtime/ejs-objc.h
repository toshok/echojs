/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_objc_h_
#define _ejs_objc_h_

#include "ejs-object.h"

#if !OBJC
typedef void* id;
#endif

// a handle type, basically an EJSObject that wraps an id so we can marshal it to JS
typedef struct {
    /* object header */
    EJSObject obj;

    /* handle data */
    id handle;
} EJSObjcHandle;

EJS_BEGIN_DECLS

extern ejsval _ejs_ObjcHandle;
extern ejsval _ejs_ObjcHandle_proto;
extern EJSSpecOps _ejs_objchandle_specops;

extern ejsval _ejs_objc_handle_new (id handle);


extern ejsval _ejs_CoffeeKitObject;
extern ejsval _ejs_CoffeeKitObject_proto;
extern EJSSpecOps _ejs_coffeekitobject_specops;

extern void _ejs_objc_init(ejsval global);

// the module handler, so we can do "objc = require 'objc'"
ejsval _ejs_objc_module_func (ejsval env, ejsval _this, uint32_t argc, ejsval* args);

EJS_END_DECLS

#endif /* _ejs_objc_h */
