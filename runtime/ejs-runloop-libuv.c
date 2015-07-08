#if HAVE_LIBUV

#include "ejs-runloop.h"

#include "uv.h"

typedef struct {
    uv_timer_t timer;

    Task task;
    void *data;
    TaskDataDtor dtor;
} task_timer;

static void invoke_task(uv_timer_t *timer, int unused) {
    task_timer *t = (task_timer *)timer->data;
    t->task(t->data);
    t->dtor(t->data);
    uv_timer_stop(timer);
    free(t);
}

void _ejs_runloop_add_task(Task task, void *data, TaskDataDtor dtor) {
    task_timer *t = malloc(sizeof(task_timer));
    uv_timer_init(uv_default_loop(), &t->timer);
    t->timer.data = t;
    t->task = task;
    t->data = data;
    t->dtor = dtor;

    uv_timer_start(&t->timer, invoke_task, 0, 0);
}

void _ejs_runloop_start() { uv_run(uv_default_loop(), UV_RUN_DEFAULT); }

#endif // HAVE_LIBUV
