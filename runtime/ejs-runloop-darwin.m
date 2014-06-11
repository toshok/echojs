#import <Foundation/Foundation.h>
#include "ejs-runloop.h"

static int refcount = 0;

@interface TaskObj : NSObject {
  Task task;
  void* data;
  void* runloop;
}
-(id)initWithTask:(Task)task data:(void*)data;
-(void)runTask;
@end

@implementation TaskObj
-(id)initWithTask:(Task)t data:(void*)d
{
  self = [super init];
  task = t;
  data = d;
  return self;
}

-(void)runTask
{
  task(data);
  refcount--;
  if (refcount == 0)
    exit(0);
}

@end


void
_ejs_runloop_add_task(Task task, void* data)
{
  NSRunLoop* mainloop = [NSRunLoop currentRunLoop];

  id taskObj = [[TaskObj alloc] initWithTask:task data:data];
  [mainloop performSelector:@selector(runTask) target:taskObj argument:nil order:0 modes:[NSArray arrayWithObject:NSDefaultRunLoopMode]];
  refcount++;
}

void
_ejs_runloop_start()
{
  NSRunLoop* mainloop = [NSRunLoop currentRunLoop];

  while (refcount > 0 && [mainloop runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]]) printf ("refcount = %d\n", refcount);
}
