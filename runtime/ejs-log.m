#include <stdarg.h>

#include "ejs.h"

#include <Foundation/NSObjCRuntime.h>
#include <Foundation/NSString.h>

void
_ejs_log (const char* fmt, ...)
{
  va_list va;

  va_start(va, fmt);

  NSLogv ([NSString stringWithUTF8String:fmt], va);

  va_end(va);
}

void
_ejs_logstr (const char* str)
{
  NSLog (@"%s", str);
}
