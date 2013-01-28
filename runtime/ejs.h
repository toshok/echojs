/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_h_
#define _ejs_h_

#include <stdio.h>
#include <stdlib.h>

#if __cplusplus
#define EJS_BEGIN_DECLS extern "C" {
#define EJS_END_DECLS };
#else
#define EJS_BEGIN_DECLS
#define EJS_END_DECLS
#endif

#define EJS_MACRO_START do {
#define EJS_MACRO_END } while (0)

#define EJS_STRINGIFY(x) #x

#include "ejs-types.h"

typedef int32_t EJSBool;
typedef struct _EJSContext* EJSContext;

#define EJS_TRUE 1
#define EJS_FALSE 0

#ifndef MIN
#define MIN(a,b) ((a) > (b) ? (b) : (a))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define EJS_NOT_IMPLEMENTED() EJS_MACRO_START                           \
    printf ("%s:%s:%d not implemented.\n", __PRETTY_FUNCTION__, __FILE__, __LINE__); \
    abort();                                                            \
    EJS_MACRO_END

#define EJS_NOT_REACHED() EJS_MACRO_START                               \
    printf ("%s:%s:%d should not be reached.\n", __PRETTY_FUNCTION__, __FILE__, __LINE__); \
    abort();                                                            \
    EJS_MACRO_END

typedef struct _EJSPrimString EJSPrimString;
typedef struct _EJSObject EJSObject;

#include "ejsval.h"
#include "ejs-value.h"

void _ejs_init(int argc, char** argv);

extern const ejsval _ejs_undefined;
extern const ejsval _ejs_null;
extern ejsval _ejs_nan;
extern const ejsval _ejs_true;
extern const ejsval _ejs_false;
extern const ejsval _ejs_zero;
extern const ejsval _ejs_one;
extern const ejsval _ejs_false;
extern ejsval _ejs_global;

#define EJS_ATOM(atom) extern ejsval _ejs_atom_##atom; extern const jschar _ejs_ucs2_##atom[];
#define EJS_ATOM2(atom,atom_name) extern ejsval _ejs_atom_##atom_name; extern const jschar _ejs_ucs2_##atom_name[];
#include "ejs-atoms.h"
#undef EJS_ATOM

#endif // _ejs_h_
