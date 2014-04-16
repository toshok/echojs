/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <stdio.h>
#include <string.h>
#include <libgen.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/param.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-array.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-builtin-modules.h"
#include "ejs-error.h"

////
/// path module
///

static ejsval
_ejs_path_dirname (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval path = args[0];
    // FIXME node's implementation allows a second arg to strip the extension, but the compiler doesn't use it.
    char *utf8_path = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(path));
    ejsval rv = _ejs_string_new_utf8(dirname (utf8_path));
    free(utf8_path);
    return rv;
}

static ejsval
_ejs_path_basename (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval path = args[0];
    // FIXME node's implementation allows a second arg to strip the extension, but the compiler doesn't use it.
    char *utf8_path = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(path));
    ejsval rv = _ejs_string_new_utf8(basename (utf8_path));
    free (utf8_path);
    return rv;
}

static char*
resolve(char* from, char* to)
{
    int rv_len = strlen(from) + strlen(to) + 2;
    char* rv = malloc(rv_len);
    memset(rv, 0, rv_len);

    strcat(rv, from);
    strcat(rv, "/");
    strcat(rv, to);

    return rv;
}

static ejsval
_ejs_path_resolve (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    // FIXME node's implementation is a lot more flexible.  we just combine the paths with a / between them.
    ejsval from = _ejs_undefined;
    ejsval to = _ejs_undefined;

    if (argc > 0) from = args[0];
    if (argc > 1) to = args[1];

    if (!EJSVAL_IS_STRING(from) || !EJSVAL_IS_STRING(to))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Arguments to path.resolve must be strings");

    char *from_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(from));
    char *to_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(to));

    char* resolved = resolve(from_utf8, to_utf8);

    ejsval rv = _ejs_string_new_utf8(resolved);

    free (from_utf8);
    free (to_utf8);
    free (resolved);

    return rv;
}

static char*
make_absolute(char* path)
{
    char cwd[MAXPATHLEN];
    getcwd(cwd, MAXPATHLEN);

    char* rv = resolve (cwd, path);
    free (path);

    return rv;
}

static ejsval
_ejs_path_relative (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval from = _ejs_undefined;
    ejsval to   = _ejs_undefined;

    if (argc > 0) from = args[0];
    if (argc > 1) to   = args[1];

    if (!EJSVAL_IS_STRING(from) || !EJSVAL_IS_STRING(to))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Arguments to path.relative must be strings");

    char *from_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(from));
    char *to_utf8   = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(to));

    if (from_utf8[0] != '/') from_utf8 = make_absolute(from_utf8);
    if (to_utf8[0]   != '/') to_utf8   = make_absolute(to_utf8);

    char* p = to_utf8 + strlen(to_utf8) - 1;
    int up = 0;
    EJSBool seen_slash = EJS_FALSE;

    while (p != to_utf8) {
        if (*p == '/') {
            if (seen_slash) continue; // skip adjacent slashes
            seen_slash = EJS_TRUE;
            char* prefix = strndup(to_utf8, p - to_utf8);
            if (strstr(from_utf8, prefix) == from_utf8) {
                free (prefix);
                goto done;
            }
            free (prefix);
            up ++;
        }
        else {
            seen_slash = EJS_FALSE;
        }
        p--;
    }
    // we made it all the way to the end, fall through to building up our string

 done:
    {
        ejsval dotdotslash = _ejs_string_new_utf8("../");
        ejsval rv = _ejs_string_new_utf8(p+1);
        while (up >= 0) {
            rv = _ejs_string_concat(dotdotslash, rv);
            up--;
        }

        free (from_utf8);
        free (to_utf8);

        return rv;
    }
}

ejsval
_ejs_path_module_func (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval exports = args[0];

    EJS_INSTALL_FUNCTION(exports, "dirname", _ejs_path_dirname);
    EJS_INSTALL_FUNCTION(exports, "basename", _ejs_path_basename);
    EJS_INSTALL_FUNCTION(exports, "resolve", _ejs_path_resolve);
    EJS_INSTALL_FUNCTION(exports, "relative", _ejs_path_relative);

    return _ejs_undefined;
}

////
/// fs module
///

static ejsval
_ejs_fs_readFileSync (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    // FIXME we currently ignore the encoding and just slam the entire thing into a buffer and return a utf8 string...
    char* utf8_path = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(args[0]));

    int fd = open (utf8_path, O_RDONLY);
    free(utf8_path);

    struct stat fd_stat;

    fstat (fd, &fd_stat);

    char *buf = (char*)malloc (fd_stat.st_size);
    read(fd, buf, fd_stat.st_size);
    close(fd);

    ejsval rv = _ejs_string_new_utf8_len(buf, fd_stat.st_size);
    free(buf);
    return rv;
}

ejsval
_ejs_stream_write (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval to_write = ToString(args[0]);
    ejsval internal_fd = _ejs_object_getprop_utf8 (_this, "%internal_fd");
    int fd = ToInteger(internal_fd);

    int remaining = EJSVAL_TO_STRLEN(to_write);
    int offset = 0;
    char *buf = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(to_write));
    
    do {
        int num_written = write (fd, buf + offset, remaining);
        if (num_written == -1) {
            if (errno == EINTR)
                continue;
            perror ("write");
            free (buf);
            return _ejs_false;
        }
        remaining -= num_written;
        offset += num_written;
    } while (remaining > 0);

    free (buf);
    return _ejs_true;
}

ejsval
_ejs_stream_end (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval internal_fd = _ejs_object_getprop_utf8 (_this, "%internal_fd");
    close (ToInteger(internal_fd));
    return _ejs_undefined;
}

static ejsval
_ejs_wrapFdWithStream (int fd)
{
    ejsval stream = _ejs_object_create(_ejs_null);

    EJS_INSTALL_FUNCTION (stream, "write", _ejs_stream_write);
    EJS_INSTALL_FUNCTION (stream, "end", _ejs_stream_end);

    _ejs_object_setprop_utf8 (stream, "%internal_fd", NUMBER_TO_EJSVAL(fd));

    return stream;
}

ejsval
_ejs_fs_createWriteStream (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    char *utf8_path = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(args[0]));

    int fd = open (utf8_path, O_CREAT | O_TRUNC | O_WRONLY, 0777);
    free (utf8_path);
    if (fd == -1) {
        perror ("open");
        printf ("we should totally throw an exception here\n");
        return _ejs_undefined;
    }

    return _ejs_wrapFdWithStream(fd);  
}

ejsval
_ejs_fs_module_func (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval exports = args[0];

    EJS_INSTALL_FUNCTION(exports, "readFileSync", _ejs_fs_readFileSync);
    EJS_INSTALL_FUNCTION(exports, "createWriteStream", _ejs_fs_createWriteStream);

    return _ejs_undefined;
}


static ejsval
_ejs_child_process_spawn (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    char* argv0 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(args[0]));
    EJSArray* argv_rest = (EJSArray*)EJSVAL_TO_OBJECT(args[1]);

    char **argv = (char**)calloc(sizeof(char*), EJSARRAY_LEN(argv_rest) + 2);
    argv[0] = argv0;
    for (uint32_t i = 0; i < EJSARRAY_LEN(argv_rest); i ++)
        argv[1+i] = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(ToString(EJSDENSEARRAY_ELEMENTS(argv_rest)[i])));

    pid_t pid;
    switch (pid = fork()) {
    case -1: /* error */
        perror("fork");
        printf ("we should totally throw an exception here\n");
        break;
    case 0:  /* child */
        execvp (argv0, argv);
        perror("execv");
        EJS_NOT_REACHED();
        break;
    default: /* parent */ {
        int stat;
        int wait_rv;
        do {
            wait_rv = waitpid(pid, &stat, 0);
        } while (wait_rv == -1 && errno == EINTR);

        if (wait_rv != pid) {
            perror ("waitpid");
            printf ("we should totally throw an exception here\n");
        }
        break;
    }
    }
    for (uint32_t i = 0; i < EJSARRAY_LEN(argv_rest)+1; i ++)
        free (argv[i]);
    free (argv);
    return _ejs_undefined;
}

ejsval
_ejs_child_process_module_func (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval exports = args[0];

    EJS_INSTALL_FUNCTION(exports, "spawn", _ejs_child_process_spawn);

    _ejs_object_setprop_utf8 (exports, "stdout", _ejs_wrapFdWithStream(1));
    _ejs_object_setprop_utf8 (exports, "stderr", _ejs_wrapFdWithStream(2));

    return _ejs_undefined;
}
