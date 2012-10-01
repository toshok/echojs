
#ifndef _ejs_uri_h
#define _ejs_uri_h

#include "ejs.h"
#include "ejs-object.h"

EJS_BEGIN_DECLS

EJSValue* _ejs_decodeURI (EJSValue *env, EJSValue* _this, int argc, EJSValue** args);
EJSValue* _ejs_decodeURIComponent (EJSValue *env, EJSValue* _this, int argc, EJSValue** args);

EJSValue* _ejs_encodeURI (EJSValue *env, EJSValue* _this, int argc, EJSValue** args);
EJSValue* _ejs_encodeURIComponent (EJSValue *env, EJSValue* _this, int argc, EJSValue** args);

EJS_END_DECLS

#endif
