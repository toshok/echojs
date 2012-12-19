
#ifndef _ejs_uri_h
#define _ejs_uri_h

#include "ejs.h"
#include "ejs-object.h"

EJS_BEGIN_DECLS

ejsval _ejs_decodeURI (ejsval env, ejsval _this, int argc, ejsval* args);
ejsval _ejs_decodeURIComponent (ejsval env, ejsval _this, int argc, ejsval* args);

ejsval _ejs_encodeURI (ejsval env, ejsval _this, int argc, ejsval* args);
ejsval _ejs_encodeURIComponent (ejsval env, ejsval _this, int argc, ejsval* args);

EJS_END_DECLS

#endif
