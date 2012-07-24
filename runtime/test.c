#include "ejs.h"
#include "object.h"
#include "ops.h"

int
main(int argc, char** argv)
{
  EJSValue* val1 = ejs_number_new (8);
  EJSValue* val2 = ejs_number_new (8);
  EJSValue* result;

  ejs_op_add (val1, val2, &result);
  printf ("8+8=");
  ejs_print (result);
  printf ("\n");
  return 0;
}
