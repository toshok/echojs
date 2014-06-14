#include "ejs-runloop.h"
#include "ejs.h"

#if HAVE_LIBUV

void
_ejs_runloop_add_task(Task task, void* data, TaskDataDtor dtor)
{
  EJS_NOT_IMPLEMENTED();
}

void
_ejs_runloop_start()
{
  EJS_NOT_IMPLEMENTED();
}

#endif // HAVE_LIBUV
