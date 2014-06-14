#include "ejs-runloop.h"
#include "ejs.h"

#if IOS || OSX
#import <Foundation/Foundation.h>

static int refcount = 0;

@interface TaskObj : NSObject {
  Task task;
  TaskDataDtor dtor;
  void* data;
}
-(id)initWithTask:(Task)task data:(void*)data dtor:(TaskDataDtor)dtor;
-(void)runTask;
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

-(void)runTask
{
  task(data);
  dtor(data);
  refcount--;
  if (refcount == 0)
    exit(0);
}

@end


void
_ejs_runloop_add_task(Task task, void* data, TaskDataDtor dtor)
{
  NSRunLoop* mainloop = [NSRunLoop currentRunLoop];

  id taskObj = [[TaskObj alloc] initWithTask:task data:data dtor:dtor];
  [mainloop performSelector:@selector(runTask) target:taskObj argument:nil order:0 modes:[NSArray arrayWithObject:NSDefaultRunLoopMode]];
  refcount++;
}

void
_ejs_runloop_start()
{
  NSRunLoop* mainloop = [NSRunLoop currentRunLoop];

  while (refcount > 0 && [mainloop runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]]) printf ("refcount = %d\n", refcount);
}
#endif
