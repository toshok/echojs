/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_module_h_
#define _ejs_module_h_

#include "ejs-object.h"
#include "ejs-function.h"

typedef struct {
    /* object header */
    EJSObject obj;

    /* regexp specific data */
    const char* module_name;

    int num_exports;
    ejsval exports[1];
} EJSModule;

typedef ejsval (*ExternalModuleEntry) (ejsval module_obj); // return type should be void?
typedef struct {
    const char *name;
    ExternalModuleEntry func;
    ejsval cached_module_obj EJSVAL_ALIGNMENT;
} EJSExternalModule;

EJS_BEGIN_DECLS

extern ejsval _ejs_Module;
extern ejsval _ejs_Module_prototype;
extern EJSSpecOps _ejs_Module_specops;

extern EJSModule* _ejs_modules[];
extern int _ejs_num_modules;

ejsval* _ejs_module_get_slot_ref (EJSModule* module, int slot);

void _ejs_module_add_export_accessors (EJSModule* module, const char *ident, EJSClosureFunc getter, EJSClosureFunc setter);

EJS_END_DECLS

#endif /* _ejs_module_h */
