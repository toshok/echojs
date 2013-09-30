/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <stdio.h>
#include <libgen.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-array.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-builtin-modules.h"

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

static ejsval
_ejs_path_resolve (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    // FIXME node's implementation is a lot more flexible.  we just combine the paths with a / between them.
    ejsval from = args[0];
    ejsval to = args[1];
    return _ejs_string_concat (from, _ejs_string_concat (_ejs_string_new_utf8("/"), to));
}

ejsval
_ejs_path_module_func (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval exports = args[0];

    EJS_INSTALL_FUNCTION(exports, "dirname", _ejs_path_dirname);
    EJS_INSTALL_FUNCTION(exports, "basename", _ejs_path_basename);
    EJS_INSTALL_FUNCTION(exports, "resolve", _ejs_path_resolve);

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
            return _ejs_undefined;
        }
        remaining -= num_written;
        offset += num_written;
    } while (remaining > 0);

    free (buf);
    return _ejs_undefined;
}

ejsval
_ejs_stream_end (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval internal_fd = _ejs_object_getprop_utf8 (_this, "%internal_fd");
    close (ToInteger(internal_fd));
    return _ejs_undefined;
}

ejsval
_ejs_fs_createWriteStream (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval stream = _ejs_object_create(_ejs_null);

    EJS_INSTALL_FUNCTION (stream, "write", _ejs_stream_write);
    EJS_INSTALL_FUNCTION (stream, "end", _ejs_stream_end);

    char *utf8_path = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(args[0]));

    int fd = open (utf8_path, O_CREAT | O_TRUNC | O_WRONLY, 0777);
    free (utf8_path);
    if (fd == -1) {
        perror ("open");
        printf ("we should totally throw an exception here\n");
        return _ejs_undefined;
    }

    _ejs_object_setprop_utf8 (stream, "%internal_fd", NUMBER_TO_EJSVAL(fd));

    return stream;
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
    for (int i = 0; i < EJSARRAY_LEN(argv_rest); i ++)
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
    for (int i = 0; i < EJSARRAY_LEN(argv_rest)+1; i ++)
        free (argv[i]);
    free (argv);
    return _ejs_undefined;
}

ejsval
_ejs_child_process_module_func (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval exports = args[0];

    EJS_INSTALL_FUNCTION(exports, "spawn", _ejs_child_process_spawn);

    return _ejs_undefined;
}
