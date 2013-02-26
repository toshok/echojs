/* -*- Mode: objc; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs.h"
#include "ejs-gc.h"
#include "ejs-value.h"
#include "ejs-require.h"
#include "ejs-function.h"
#include "ejs-string.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>


#define GC_ON_SHUTDOWN 0

extern const char *entry_filename;

int
main(int argc, char** argv)
{
    //@autoreleasepool {

        EJS_GC_MARK_THREAD_STACK_BOTTOM;

        _ejs_init(argc, argv);

        START_SHADOW_STACK_FRAME;

        ADD_STACK_ROOT(ejsval, entry_name, _ejs_string_new_utf8(entry_filename));

        _ejs_invoke_closure_1 (_ejs_require, _ejs_null, 1, entry_name);

        END_SHADOW_STACK_FRAME;

        UIApplicationMain (argc, argv, NULL, NULL);

#if GC_ON_SHUTDOWN
        _ejs_gc_shutdown();
#endif
        return 0;

    //}
}
