/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_object_h_
#define _ejs_object_h_

#include "ejs.h"
#include "ejs-gc.h"
#include "ejs-value.h"

// really terribly performing property maps
struct _EJSPropertyMap {
    char **names;
    ejsval *fields;
    int allocated;
    int num;
};

typedef struct _EJSPropertyMap EJSPropertyMap;
typedef struct _EJSPropertyIterator EJSPropertyIterator;

typedef struct {
    EJSBool configurable;
    EJSBool writable;
    EJSBool enumerable;
} EJSPropertyDesc;

typedef ejsval  (*SpecOpGet) (ejsval obj, ejsval propertyName, EJSBool isCStr);
typedef ejsval  (*SpecOpGetOwnProperty) (ejsval obj, ejsval propertyName);
typedef ejsval  (*SpecOpGetProperty) (ejsval obj, ejsval propertyName);
typedef void    (*SpecOpPut) (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag);
typedef EJSBool (*SpecOpCanPut) (ejsval obj, ejsval propertyName);
typedef EJSBool (*SpecOpHasProperty) (ejsval obj, ejsval propertyName);
typedef EJSBool (*SpecOpDelete) (ejsval obj, ejsval propertyName, EJSBool flag);
typedef ejsval  (*SpecOpDefaultValue) (ejsval obj, const char *hint);
typedef void    (*SpecOpDefineOwnProperty) (ejsval obj, ejsval propertyName, ejsval propertyDescriptor, EJSBool flag);
typedef void    (*SpecOpFinalize) (EJSObject* obj);
typedef void    (*SpecOpScan) (EJSObject* obj, EJSValueFunc scan_func);

typedef struct {
    const char* class_name;
    SpecOpGet get;
    SpecOpGetOwnProperty get_own_property;
    SpecOpGetProperty get_property;
    SpecOpPut put;
    SpecOpCanPut can_put;
    SpecOpHasProperty has_property;
    SpecOpDelete _delete;
    SpecOpDefaultValue default_value;
    SpecOpDefineOwnProperty define_own_property;

    SpecOpFinalize finalize;
    SpecOpScan scan;
} EJSSpecOps;

struct _EJSObject {
    ejsval proto;
    EJSSpecOps *ops;
    EJSPropertyMap* map;
};


#define OP(o,op) (((EJSObject*)o)->ops->op)

#define CLASSNAME(o) OP(o,class_name)

EJS_BEGIN_DECLS

EJSPropertyMap* _ejs_propertymap_new (int initial_allocation);
int _ejs_propertymap_lookup (EJSPropertyMap *map, const char *name, EJSBool add_if_not_found);
void _ejs_propertymap_foreach_value (EJSPropertyMap *map, EJSValueFunc foreach_func);

ejsval _ejs_object_setprop (ejsval obj, ejsval key, ejsval value);
ejsval _ejs_object_getprop (ejsval obj, ejsval key);

ejsval _ejs_object_setprop_utf8 (ejsval obj, const char *key, ejsval value);
ejsval _ejs_object_getprop_utf8 (ejsval obj, const char *key);

EJSPropertyIterator* _ejs_property_iterator_new (ejsval forObj);
char *_ejs_property_iterator_current (EJSPropertyIterator* iterator);
void _ejs_property_iterator_next (EJSPropertyIterator* iterator);
void _ejs_property_iterator_free (EJSPropertyIterator *iterator);

extern ejsval _ejs_Object;
extern ejsval _ejs_Object_proto;
extern EJSSpecOps _ejs_object_specops;

ejsval _ejs_object_new (ejsval proto);
EJSObject* _ejs_object_alloc_instance();
void      _ejs_init_object (EJSObject *obj, ejsval proto, EJSSpecOps *ops);

void _ejs_object_finalize(EJSObject *obj);

void _ejs_object_init(ejsval global);

EJS_END_DECLS

#endif // _ejs_object_h_
