typedef void (*Task)(void* data);

void _ejs_runloop_add_task(Task task, void* data);
void _ejs_runloop_start();
