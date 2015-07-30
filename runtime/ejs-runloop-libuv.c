#if HAVE_LIBUV

#include "ejs-runloop.h"

#include "uv.h"

typedef struct {
  uv_timer_t timer;

  Task task;
  void* data;
  TaskDataDtor dtor;
  EJSBool repeats;
} task_timer;

static void
invoke_task(uv_timer_t* timer, int unused)
{
  task_timer* t = (task_timer*)timer->data;
  t->task(t->data);
  if (t->repeats)
    return;
  t->dtor(t->data);
  uv_timer_stop(timer);
  free(t);
}

void
_ejs_runloop_add_task(Task task, void* data, TaskDataDtor dtor)
{
  task_timer* t = malloc(sizeof(task_timer));
  uv_timer_init(uv_default_loop(), &t->timer);
  t->timer.data = t;
  t->task = task;
  t->data = data;
  t->dtor = dtor;

  uv_timer_start(&t->timer, invoke_task, 0, 0);
}

void*
_ejs_runloop_add_task_timeout(Task task, void* data, TaskDataDtor dtor, int64_t timeout, EJSBool repeats)
{
  task_timer* t = malloc(sizeof(task_timer));
  uv_timer_init(uv_default_loop(), &t->timer);
  t->timer.data = t;
  t->task = task;
  t->data = data;
  t->dtor = dtor;
  t->repeats = repeats;

  if (timeout == 0)
    timeout = 1;

  uint64_t repeat_ms = repeats ? timeout : 0;

  uv_timer_start(&t->timer, invoke_task, timeout, repeat_ms);
  return t;
}

void
_ejs_runloop_remove_task(void* handle)
{
  task_timer* t = (task_timer*)handle;
  uv_timer_stop(&t->timer);
  uv_unref((uv_handle_t*)&t->timer);
}

void
_ejs_runloop_start()
{
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}

#endif // HAVE_LIBUV
