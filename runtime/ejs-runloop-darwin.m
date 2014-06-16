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
-(void)addToRunLoop;
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

-(void)addToRunLoop
{
  refcount++;
  [self performSelectorOnMainThread:@selector(runTask) withObject:self waitUntilDone:NO];
}

-(void)runTask
{
  task(data);
  if (dtor)
      dtor(data);
  refcount--;
  if (refcount == 0)
    exit(0);
}

@end


void
_ejs_runloop_add_task(Task task, void* data, TaskDataDtor dtor)
{
  id taskObj = [[TaskObj alloc] initWithTask:task data:data dtor:dtor];
  [taskObj addToRunLoop];
}

void
_ejs_runloop_start()
{
  if (refcount == 0)
    return;

  [[NSRunLoop currentRunLoop] run];
}
#endif
