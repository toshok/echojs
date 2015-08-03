#include "ejs-runloop.h"

void
_ejs_runloop_add_task(Task task, void* data, TaskDataDtor dtor)
{
}

void*
_ejs_runloop_add_task_timeout(Task task, void* data, TaskDataDtor dtor, int64_t timeout, EJSBool repeats)
{
  return NULL;
}

void
_ejs_runloop_remove_task(void* handle)
{
}

void
_ejs_runloop_start()
{
}
