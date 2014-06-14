typedef void (*Task)(void* data);
typedef void (*TaskDataDtor)(void* data);

void _ejs_runloop_add_task(Task task, void* data, TaskDataDtor data_dtor);
void _ejs_runloop_start();
