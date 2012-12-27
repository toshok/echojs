
#ifndef _ejs_h_
#define _ejs_h_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if __cplusplus
#define EJS_BEGIN_DECLS extern "C" {
#define EJS_END_DECLS };
#else
#define EJS_BEGIN_DECLS
#define EJS_END_DECLS
#endif

#define EJS_MACRO_START do {
#define EJS_MACRO_END } while (0)

typedef char EJSBool;
typedef struct _EJSContext* EJSContext;

#define EJS_TRUE 1
#define EJS_FALSE 0

#ifndef MIN
#define MIN(a,b) (a) > (b) ? (b) : (a)
#endif

#ifndef MAX
#define MAX(a,b) (a) > (b) ? (a) : (b)
#endif

#define NOT_IMPLEMENTED() EJS_MACRO_START \
  printf ("%s not implemented.\n", __PRETTY_FUNCTION__);	\
  abort();							\
  EJS_MACRO_END

#include "ejs-types.h"

typedef struct _EJSPrimString EJSPrimString;
typedef struct _EJSObject EJSObject;

#include "ejsval.h"
#include "ejs-value.h"

void _ejs_init(int argc, char** argv);

extern ejsval _ejs_undefined;
extern ejsval _ejs_null;
extern ejsval _ejs_nan;
extern ejsval _ejs_true;
extern ejsval _ejs_false;
extern ejsval _ejs_global;
extern ejsval _ejs_length;

#endif // _ejs_h_
