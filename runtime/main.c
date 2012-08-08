#include "ejs.h"
#include "object.h"

extern int* _ejs_script(void* __ejs_context, void *__ejs_this);

int
main(int argc, char* argv)
{
  _ejs_init();
  _ejs_script(NULL, _ejs_object_new(NULL));
}
