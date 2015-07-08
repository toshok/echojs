#include "ejs-runloop.h"
#include "ejs.h"

#if IOS || OSX
#import <Foundation/Foundation.h>

int _ejs_runloop_darwin_refcount = 0;

@interface TaskObj : NSObject {
    Task task;
    TaskDataDtor dtor;
    void *data;
}
- (id)initWithTask:(Task)task data:(void *)data dtor:(TaskDataDtor)dtor;
- (void)addToRunLoop;
- (void)runTask;
@end

@implementation TaskObj
- (id)initWithTask:(Task)t data:(void *)d dtor:(TaskDataDtor)dt {
    self = [super init];
    task = t;
    data = d;
    dtor = dt;
    return self;
}

- (void)addToRunLoop {
    _ejs_runloop_darwin_refcount++;
    [self performSelectorOnMainThread:@selector(runTask)
                           withObject:self
                        waitUntilDone:NO];
}

- (void)runTask {
    task(data);
    if (dtor)
        dtor(data);
    _ejs_runloop_darwin_refcount--;
    if (_ejs_runloop_darwin_refcount == 0)
        exit(0);
}

@end

void _ejs_runloop_add_task(Task task, void *data, TaskDataDtor dtor) {
    id taskObj = [[TaskObj alloc] initWithTask:task data:data dtor:dtor];
    [taskObj addToRunLoop];
}

void _ejs_runloop_start() {
    if (_ejs_runloop_darwin_refcount == 0)
        return;

    while (_ejs_runloop_darwin_refcount > 0 &&
           [[NSRunLoop mainRunLoop] runMode:NSDefaultRunLoopMode
                                 beforeDate:[NSDate distantFuture]])
        ;
}
#endif
