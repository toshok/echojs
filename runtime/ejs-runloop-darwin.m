#include "ejs-runloop.h"
#include "ejs.h"

#if IOS || OSX
#import <Foundation/Foundation.h>

int _ejs_runloop_darwin_refcount = 0;

@interface TaskObj : NSObject {
  Task task;
  TaskDataDtor dtor;
  void* data;
}
-(id)initWithTask:(Task)task data:(void*)data dtor:(TaskDataDtor)dtor;
-(void)addToRunLoop;
-(void)runTask;
@end

@interface DelayedTaskObj : NSObject {
    Task task;
    TaskDataDtor dtor;
    void* data;
    NSTimeInterval interval;
    BOOL repeatTask;
}
-(id)initWithTask:(Task)task data:(void*)data dtor:(TaskDataDtor)dtor interval:(NSTimeInterval)interval repeats:(BOOL)repeats;
-(NSTimer*)addToRunLoop;
-(void)stopDelayedTask:(NSTimer*)timer;
@end

@implementation TaskObj
-(id)initWithTask:(Task)t data:(void*)d dtor:(TaskDataDtor)dt
{
  self = [super init];
  task = t;
  data = d;
  dtor = dt;
  return self;
}

-(void)addToRunLoop
{
  _ejs_runloop_darwin_refcount++;
  [self performSelectorOnMainThread:@selector(runTask) withObject:self waitUntilDone:NO];
}

-(void)runTask
{
  task(data);
  if (dtor)
      dtor(data);
  _ejs_runloop_darwin_refcount--;
  if (_ejs_runloop_darwin_refcount == 0)
    exit(0);
}
@end

@implementation DelayedTaskObj
-(id)initWithTask:(Task)t data:(void*)d dtor:(TaskDataDtor)dt interval:(NSTimeInterval)i repeats:(BOOL)repeats
{
    self = [super init];
    task = t;
    data = d;
    dtor = dt;
    interval = i;
    repeatTask = repeats;

    return self;
}

-(NSTimer*)addToRunLoop
{
    NSTimer *timer = [NSTimer timerWithTimeInterval:interval
                                         target:self
                                       selector:@selector(runDelayedTask:)
                                       userInfo:self
                                        repeats:repeatTask];

    [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSDefaultRunLoopMode];
    _ejs_runloop_darwin_refcount++;

    return timer;
}

-(void)runDelayedTask:(NSTimer*)timer
{
    task(data);
    if (repeatTask)
        return;

    if (dtor)
        dtor(data);

    [self decreaseRefCount];
}

-(void)stopDelayedTask:(NSTimer*)timer
{
    if (!timer.isValid)
        return;

    if (dtor)
        dtor(data);

    [timer invalidate];
    [self decreaseRefCount];
}

-(void)decreaseRefCount
{
    _ejs_runloop_darwin_refcount--;
    if (_ejs_runloop_darwin_refcount == 0)
        exit(0);
}
@end

void
_ejs_runloop_add_task(Task task, void* data, TaskDataDtor dtor)
{
  TaskObj *taskObj = [[TaskObj alloc] initWithTask:task data:data dtor:dtor];
  [taskObj addToRunLoop];
}

void*
_ejs_runloop_add_task_timeout(Task task, void* data, TaskDataDtor dtor, int64_t timeout, EJSBool repeats)
{
    BOOL repeatTask = repeats == EJS_TRUE;
    NSTimeInterval interval = timeout / 1000.0;
    DelayedTaskObj *taskObj = [[DelayedTaskObj alloc] initWithTask:task
                                                              data:data
                                                              dtor:dtor
                                                          interval:interval
                                                           repeats:repeatTask];
    return [taskObj addToRunLoop];
}

void
_ejs_runloop_remove_task(void* handle)
{
    NSTimer *timer = (NSTimer*)handle;
    DelayedTaskObj *taskObj = timer.userInfo;
    [taskObj stopDelayedTask:timer];
}

void
_ejs_runloop_start()
{
  if (_ejs_runloop_darwin_refcount == 0)
    return;

  while (_ejs_runloop_darwin_refcount > 0 && [[NSRunLoop mainRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]]) ;
}
#endif
