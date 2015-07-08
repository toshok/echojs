/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_proxy_h_
#define _ejs_proxy_h_

#include "ejs.h"
#include "ejs-value.h"
#include "ejs-object.h"

#define EJSVAL_IS_PROXY(v)                                                     \
    (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_Proxy_specops))
#define EJSVAL_TO_PROXY(v) ((EJSProxy *)EJSVAL_TO_OBJECT(v))

typedef struct {
    /* object header */
    EJSObject obj;

    ejsval handler; // [[ProxyHandler]]
    ejsval target;  // [[ProxyTarget]]

    // traps which, if overridden, are called from a proxy's
    // specops. if they aren't overridden, the equivalent specop is
    // invoked on the target object.
    ejsval callTrap;
    ejsval constructTrap;
} EJSProxy;

EJS_BEGIN_DECLS

extern ejsval _ejs_Proxy;
extern ejsval _ejs_Proxy_prototype;
extern EJSSpecOps _ejs_Proxy_specops;

void _ejs_proxy_init(ejsval global);

void _ejs_proxy_new(ejsval handler, ejsval target);
void _ejs_proxy_new_function(ejsval handler, ejsval target, ejsval callTrap,
                             ejsval constructTrap);

EJS_END_DECLS

#endif
