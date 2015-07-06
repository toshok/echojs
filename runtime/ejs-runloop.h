/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_runloop_h_
#define _ejs_runloop_h_

#include "ejs.h"

EJS_BEGIN_DECLS

typedef void (*Task)(void* data);
typedef void (*TaskDataDtor)(void* data);

void _ejs_runloop_add_task(Task task, void* data, TaskDataDtor data_dtor);
void* _ejs_runloop_add_task_timeout(Task task, void* data, TaskDataDtor data_dtor, int64_t timeout, EJSBool repeats);
void _ejs_runloop_remove_task(void *handle);
void _ejs_runloop_start();


EJS_END_DECLS

#endif /* _ejs_array_h */
