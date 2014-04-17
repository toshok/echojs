/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <assert.h>

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
resolvev(char** paths, int num_paths)
{
    if (num_paths == 1) return strdup(paths[0]);

    char* stack[MAXPATHLEN];
    memset(stack, 0, sizeof(char*) * MAXPATHLEN);
    int sp = 0;

    // we treat paths as a stack of path elements.  walk all the path
    // elements of a given path, popping for '..', doing nothing for
    // '.', and pushing for anything else.
    for (int i = 0; i < num_paths; i ++) {
        char* p = paths[i];
        if (*p == '/') {
            // this should only be true for the first path
            assert(i == 0);
            while (*p == '/') p++; // consume all /'s at the start of the path
        }

        while (*p) {
            if (*p == '.') {
                if (*(p+1) == '.' && *(p+2) == '/') {
                    if (sp) {
                        if (stack[sp])
                            free (stack[sp]);
                        sp--;
                    }
                    p += 3;
                    while (*p == '/') p++; // consume all adjacent /'s
                    continue;
                }
                else if (*(p+1) == '/') {
                    p += 2;
                    while (*p == '/') p++; // consume all adjacent /'s
                    continue;
                }
            }

            char component[MAXPATHLEN];
            memset(component, 0, sizeof(component));
            int c = 0;
            while (*p && *p != '/') {
                component[c++] = *p++;
            }
            if (*p == '/') while (*p == '/') p++; // consume all adjacent /'s
            if (c > 0) {
                stack[sp++] = strdup(component);
            }
        }
    }

    // now that we're done the stack contains the contents of the path
    char result_utf8[MAXPATHLEN];
    memset (result_utf8, 0, sizeof(result_utf8));
    char *p;

    p = result_utf8;
    for (int s = 0; s < sp; s ++) {
        *p++ = '/';
        char *c = stack[s];
        while (*c) *p++ = *c++;
    }

    for (int s = 0; s < sp; s ++)
        free(stack[s]);

    return strdup(result_utf8);
}

static char*
make_absolute(char* path)
{
    char cwd[MAXPATHLEN];
    getcwd(cwd, MAXPATHLEN);

    char* paths[2];
    paths[0] = cwd;
    paths[1] = path;

    char* rv = resolvev (paths, 2);
    free (path);

    return rv;
}

static ejsval
_ejs_path_resolve (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    char** paths_utf8 = (char**)calloc(argc + 1, sizeof(char*));
    int num_paths = 0;

    char cwd[MAXPATHLEN];
    getcwd(cwd, MAXPATHLEN);

    paths_utf8[num_paths++] = strdup(cwd);

    for (int i = 0; i < argc; i ++) {
        ejsval arg = args[i];

        if (!EJSVAL_IS_STRING(arg)) {
            for (int j = 0; j < num_paths; j ++) free(paths_utf8[j]);
            free (paths_utf8);
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Arguments to path.resolve must be strings");
        }

        paths_utf8[num_paths++] = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(arg));
    }

    int start_path;
    for (start_path = num_paths-1; start_path >= 0; start_path --) {
        if (paths_utf8[start_path][0] == '/')
            break;
    }
    // at this point paths_utf8[start_path] is our "root" for
    // resolving.  it is either the right-most absolute path in the
    // argument list, or $cwd if there wasn't an absolute path in the
    // args.

    char* resolved = resolvev(&paths_utf8[start_path], num_paths - start_path);

    ejsval rv = _ejs_string_new_utf8(resolved);

    free (resolved);
    for (int j = 0; j < num_paths; j ++) free(paths_utf8[j]);
    free (paths_utf8);

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
            if (!strcmp(from_utf8, prefix)) {
                up = -1;
                free (prefix);
                goto done;
            }
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
    if (fd == -1) {
        char buf[256];
        snprintf (buf, sizeof(buf), "%s: `%s`", strerror(errno), utf8_path);
        free(utf8_path);
        _ejs_throw_nativeerror_utf8 (EJS_ERROR, buf);
    }

    struct stat fd_stat;

    int stat_rv = fstat (fd, &fd_stat);
    if (stat_rv == -1) {
        char buf[256];
        snprintf (buf, sizeof(buf), "%s: `%s`", strerror(errno), utf8_path);
        free(utf8_path);
        close(fd);
        _ejs_throw_nativeerror_utf8 (EJS_ERROR, buf);
    }
    free(utf8_path);

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
