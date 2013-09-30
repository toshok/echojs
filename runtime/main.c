/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs.h"
#include "ejs-gc.h"
#include "ejs-value.h"
#include "ejs-require.h"
#include "ejs-function.h"
#include "ejs-string.h"

#define GC_ON_SHUTDOWN 1

extern const char *entry_filename;

#include <setjmp.h>
#include <unistd.h>

sigjmp_buf segvbuf;

static void
segv_handler(int signum)
{
    siglongjmp (segvbuf, 1);
}


int
main(int argc, char** argv)
{
    if (getenv ("EJS_WAIT_ON_SEGV")) {
        if (sigsetjmp(segvbuf, 1)) {
            printf ("attach to pid %d\n", getpid());
            while (1) sleep (100);
            abort();
        }

        signal (SIGSEGV, segv_handler);
    }

    EJS_GC_MARK_THREAD_STACK_BOTTOM;

    _ejs_init(argc, argv);

    ejsval entry_name = _ejs_string_new_utf8(entry_filename);

    _ejs_invoke_closure (_ejs_require, _ejs_null, 1, &entry_name);

#if GC_ON_SHUTDOWN
    _ejs_gc_shutdown();
#endif
    return 0;
}
