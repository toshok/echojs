#include <stdio.h>
#include <stdarg.h>

#include "ejs.h"

#include <Foundation/NSObjCRuntime.h>
#include <Foundation/NSString.h>

static FILE*   log_file;
static EJSBool check_log_file;

static void
init_log_file()
{
  if (check_log_file)
    return;

  check_log_file = EJS_TRUE;
  char* log_file_name = getenv("EJS_LOG_FILE");
  if (log_file_name)
    log_file = fopen (log_file_name, "w");
}

void
_ejs_log (const char* fmt, ...)
{
  va_list va;

  init_log_file();

  va_start(va, fmt);

  if (log_file) {
    vfprintf (log_file, fmt, va);
    fflush(log_file);
  }
  else {
    NSLogv ([NSString stringWithUTF8String:fmt], va);
  }

  va_end(va);
}

void
_ejs_logstr (const char* str)
{
  init_log_file();

  if (log_file) {
    fprintf (log_file, "%s\n", str);
    fflush(log_file);
  }
  else {
    NSLog (@"%s", str);
  }
}
