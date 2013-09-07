/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs.h"
#include "ejsval.h"

static const char*
type_name(ejsval exp)
{
    if (EJSVAL_IS_NULL(exp))
        return "null";
    else if (EJSVAL_IS_BOOLEAN(exp))
        return "boolean";
    else if (EJSVAL_IS_STRING(exp))
        return "string";
    else if (EJSVAL_IS_NUMBER(exp))
        return "number";
    else if (EJSVAL_IS_UNDEFINED(exp))
        return "undefined";
    else if (EJSVAL_IS_OBJECT(exp))
        return "object";
    else
        EJS_NOT_IMPLEMENTED();
}

void
_ejs_record_binop (int id, const char *op, ejsval left, ejsval right)
{
    printf ("for id %d, binary operator %s: left=%s, right=%s\n", id, op, type_name(left), type_name(right));
}


void
_ejs_record_assignment (int id, ejsval val)
{
    printf ("for id %d, assignment: val=%s\n", id, type_name(val));
}
