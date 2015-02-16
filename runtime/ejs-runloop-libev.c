#if HAVE_LIBEV

#include "ejs-runloop.h"

#include "ev.h"

typedef struct {
  ev_timer timer;

  Task task;
  void* data;
  TaskDataDtor dtor;
} task_timer;

static struct ev_loop* loop;

static void
invoke_task(EV_P_ ev_timer* timer, int unused)
{
  task_timer* t = (task_timer*)timer->data;
  t->task(t->data);
  t->dtor(t->data);
  ev_timer_stop(loop, timer);
  free(t);
}

void
_ejs_runloop_add_task(Task task, void* data, TaskDataDtor dtor)
{
  task_timer* t = malloc(sizeof(task_timer));
  ev_timer_init(&t->timer, invoke_task, 0, 0);
  t->timer.data = t;
  t->task = task;
  t->data = data;
  t->dtor = dtor;

  ev_timer_start(loop, &t->timer);
}

void
_ejs_runloop_start()
{
  loop = ev_default_loop (0);
  ev_run(loop, EVRUN_NOWAIT);
}

#endif // HAVE_LIBUV
