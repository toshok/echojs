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
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/param.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-array.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-symbol.h"
#include "ejs-node-compat.h"
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

static ejsval
_ejs_path_extname (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval rv = _ejs_atom_empty;
    ejsval path = args[0];
    char *utf8_path = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(path));
    char *base = strdup(basename(utf8_path));
    free (utf8_path);

    char *p = strrchr(base, '.');
    if (p != NULL)
        rv = _ejs_string_new_utf8(p);

    free(base);
    return rv;
}

static char*
resolvev(char** paths, int num_paths)
{
    char* stack[MAXPATHLEN];
    memset(stack, 0, sizeof(char*) * MAXPATHLEN);
    int sp = 0;

    // we treat paths as a stack of path elements.  walk all the path
    // elements of a given path, popping for '..', skipping '.', and
    // pushing for anything else.
    for (int i = 0; i < num_paths; i ++) {
        char* p = paths[i];
        if (*p == '/') {
            // this should only be true for the first path
            assert(i == 0);
            while (*p == '/') p++; // consume all /'s at the start of the path
        }

        while (*p) {
            if (*p == '.') {
                if (*(p+1) == '.' && (*(p+2) == '/' || *(p+2) == '\0')) {
                    if (sp) {
                        if (stack[sp])
                            free (stack[sp]);
                        sp--;
                    }
                    p += 3;
                    while (*p == '/') p++; // consume all adjacent /'s
                    continue;
                } 
                else if (*(p+1) == '/' || *(p+1) == '\0') {
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

        if (!EJSVAL_IS_STRING(arg))
            continue;

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

static ejsval
_ejs_path_join (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    if (argc == 0)
        return _ejs_atom_empty;

    // XXX terrible, just join the strings in args with '/' between them.
    ejsval rv = args[0];

    for (int i = 1; i < argc; i ++) {
        rv = _ejs_string_concat(rv, _ejs_atom_slash);
        rv = _ejs_string_concat(rv, args[i]);
    }

    _ejs_string_flatten(rv);
    return rv;
}

ejsval
_ejs_path_module_func (ejsval exports)
{
    EJS_INSTALL_FUNCTION(exports, "dirname", _ejs_path_dirname);
    EJS_INSTALL_FUNCTION(exports, "basename", _ejs_path_basename);
    EJS_INSTALL_FUNCTION(exports, "extname", _ejs_path_extname);
    EJS_INSTALL_FUNCTION(exports, "resolve", _ejs_path_resolve);
    EJS_INSTALL_FUNCTION(exports, "relative", _ejs_path_relative);
    EJS_INSTALL_FUNCTION(exports, "join", _ejs_path_join);

    return _ejs_undefined;
}

////
/// fs module
///

static ejsval
create_errno_error(int _errno, char* path)
{
    char buf[256];
    snprintf (buf, sizeof(buf), "%s: `%s`", strerror(_errno), path);
    return _ejs_nativeerror_new_utf8(EJS_ERROR, buf);
}

// free's @path before returning the exception
static void
throw_errno_error(int _errno, char* path)
{
    ejsval error = create_errno_error(_errno, path);
    free(path);
    _ejs_throw (error);
}

#define EJS_STAT_SET_IS_FILE(s, v) (*_ejs_closureenv_get_slot_ref((s), 0) = (v))
#define EJS_STAT_SET_IS_DIRECTORY(s, v) (*_ejs_closureenv_get_slot_ref((s), 1) = (v))
#define EJS_STAT_SET_IS_CHARDEV(s, v) (*_ejs_closureenv_get_slot_ref((s), 2) = (v))
#define EJS_STAT_SET_IS_SYMLINK(s, v) (*_ejs_closureenv_get_slot_ref((s), 3) = (v))
#define EJS_STAT_SET_IS_FIFO(s, v) (*_ejs_closureenv_get_slot_ref((s), 4) = (v))
#define EJS_STAT_SET_IS_SOCKET(s, v) (*_ejs_closureenv_get_slot_ref((s), 5) = (v))

#define EJS_STAT_GET_IS_FILE(s) (_ejs_closureenv_get_slot((s), 0))
#define EJS_STAT_GET_IS_DIRECTORY(s) (_ejs_closureenv_get_slot((s), 1))
#define EJS_STAT_GET_IS_CHARDEV(s) (_ejs_closureenv_get_slot((s), 2))
#define EJS_STAT_GET_IS_SYMLINK(s) (_ejs_closureenv_get_slot((s), 3))
#define EJS_STAT_GET_IS_FIFO(s) (_ejs_closureenv_get_slot((s), 4))
#define EJS_STAT_GET_IS_SOCKET(s) (_ejs_closureenv_get_slot((s), 5))

static ejsval
_ejs_fs_Stat_isFile(ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    return EJS_STAT_GET_IS_FILE(env);
}

static ejsval
_ejs_fs_Stat_isDirectory(ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    return EJS_STAT_GET_IS_DIRECTORY(env);
}

static ejsval
_ejs_fs_Stat_isCharacterDevice(ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    return EJS_STAT_GET_IS_CHARDEV(env);
}

static ejsval
_ejs_fs_Stat_isSymbolicLink(ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    return EJS_STAT_GET_IS_SYMLINK(env);
}

static ejsval
_ejs_fs_Stat_isFIFO(ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    return EJS_STAT_GET_IS_FIFO(env);
}

static ejsval
_ejs_fs_Stat_isSocket(ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    return EJS_STAT_GET_IS_SOCKET(env);
}

static ejsval
_ejs_fs_Stat_new(struct stat* sb)
{
    ejsval StatData = _ejs_closureenv_new(6);

    EJS_STAT_SET_IS_FILE      (StatData, S_ISREG(sb->st_mode) ? _ejs_true : _ejs_false);
    EJS_STAT_SET_IS_DIRECTORY (StatData, S_ISDIR(sb->st_mode) ? _ejs_true : _ejs_false);
    EJS_STAT_SET_IS_CHARDEV   (StatData, S_ISCHR(sb->st_mode) ? _ejs_true : _ejs_false);
    EJS_STAT_SET_IS_SYMLINK   (StatData, S_ISLNK(sb->st_mode) ? _ejs_true : _ejs_false);
    EJS_STAT_SET_IS_FIFO      (StatData, S_ISFIFO(sb->st_mode) ? _ejs_true : _ejs_false);
    EJS_STAT_SET_IS_SOCKET    (StatData, S_ISSOCK(sb->st_mode) ? _ejs_true : _ejs_false);

    ejsval stat = _ejs_object_create(_ejs_null);

#define STAT_FUNC(n) EJS_INSTALL_FUNCTION_ENV(stat, #n, _ejs_fs_Stat_##n, StatData)

    STAT_FUNC (isFile);
    STAT_FUNC (isDirectory);
    STAT_FUNC (isCharacterDevice);
    STAT_FUNC (isSymbolicLink);
    STAT_FUNC (isFIFO);
    STAT_FUNC (isSocket);

#undef STAT_FUNC

    return stat;
}

static ejsval
_ejs_fs_statSync (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    char* utf8_path = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(args[0]));
    struct stat sb;

    int stat_rv = stat (utf8_path, &sb);
    if (stat_rv == -1)
        throw_errno_error(errno, utf8_path);

    free(utf8_path);
    return _ejs_fs_Stat_new(&sb);
}

static ejsval
_ejs_fs_existsSync (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval path = _ejs_undefined;
    if (argc > 0) path = args[0];

    if (!EJSVAL_IS_STRING_TYPE(path)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "path must be a string");
    }

    char* utf8_path = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(ToString(args[0])));

    int access_rv = access(utf8_path, F_OK);

    free(utf8_path);
    return BOOLEAN_TO_EJSVAL(access_rv != -1);
}

static ejsval
_ejs_fs_readFileSync (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    // FIXME we currently ignore the encoding and just slam the entire thing into a buffer and return a utf8 string...
    char* utf8_path = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(args[0]));

    int fd = open (utf8_path, O_RDONLY);
    if (fd == -1)
        throw_errno_error(errno, utf8_path);

    struct stat fd_stat;

    int stat_rv = fstat (fd, &fd_stat);
    if (stat_rv == -1)
        throw_errno_error(errno, utf8_path);

    int amount_to_read = fd_stat.st_size;
    int amount_read = 0;
    char *buf = (char*)calloc (1, amount_to_read+1);
    do {
        int c = read(fd, buf + amount_read, amount_to_read);
        if (c == -1) {
            if (errno == EINTR)
                continue;

            int read_errno = errno;
            free(buf);
            close(fd);
            throw_errno_error(read_errno, utf8_path);
        }
        else {
            amount_to_read -= c;
            amount_read += c;
        }
    } while (amount_to_read > 0);

    free(utf8_path);

    ejsval rv = _ejs_string_new_utf8_len(buf, amount_read);
    free(buf);
    return rv;
}

static ejsval
_ejs_fs_writeFileSync (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    // XXX we ignore the options argument (third) entirely
    ejsval path = _ejs_undefined;
    if (argc > 0) path = args[0];

    ejsval contents = _ejs_undefined;
    if (argc > 1) contents = args[1];

    if (!EJSVAL_IS_STRING_TYPE(path)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "path must be a string");
    }

    if (!EJSVAL_IS_STRING_TYPE(contents)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "contents must be a string");
    }

    char* utf8_path = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(ToString(path)));

    int fd = open (utf8_path, O_CREAT | O_TRUNC | O_WRONLY, 0666);
    if (fd == -1)
        throw_errno_error(errno, utf8_path);

    contents = ToString(contents);
    int amount_to_write = EJSVAL_TO_STRLEN(contents);
    int amount_written = 0;
    char* utf8_contents = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(contents));
    
    printf ("need to write %d bytes\n", amount_to_write);
    do {
        int c = write(fd, utf8_contents + amount_written, amount_to_write);
        if (c == -1) {
            close(fd);
            free(utf8_contents);
            throw_errno_error(errno, utf8_path);
        }
        else {
            printf ("wrote %d bytes\n", c);
            amount_to_write -= c;
            amount_written += c;
        }
    } while (amount_to_write > 0);

    close(fd);

    free(utf8_contents);
    free(utf8_path);
    return _ejs_undefined;
}

static ejsval _ejs_internal_fd_sym EJSVAL_ALIGNMENT; 

ejsval
_ejs_stream_write (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval to_write = ToString(args[0]);
    ejsval internal_fd = _ejs_object_getprop (_this, _ejs_internal_fd_sym);
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
    ejsval internal_fd = _ejs_object_getprop (_this, _ejs_internal_fd_sym);
    close (ToInteger(internal_fd));
    return _ejs_undefined;
}

static ejsval
_ejs_wrapFdWithStream (int fd)
{
    ejsval stream = _ejs_object_create(_ejs_null);

    EJS_INSTALL_FUNCTION (stream, "write", _ejs_stream_write);
    EJS_INSTALL_FUNCTION (stream, "end", _ejs_stream_end);

    _ejs_object_setprop (stream, _ejs_internal_fd_sym, NUMBER_TO_EJSVAL(fd));

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

static int
string_to_mode(ejsval strmode)
{
    strmode = ToString(strmode);
    char *utf8_mode = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(strmode));
    int length = EJSVAL_TO_STRLEN(strmode);
    int rv = 0;
    char* p = utf8_mode + length;
    while (p >= utf8_mode) {
        if (*p >= '8') abort(); // XXX an exception of some sort?
        rv = rv * 8 + *p - '0';
        p--;
    }
    return rv;
}

ejsval
_ejs_fs_mkdirSync (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval path = _ejs_undefined;
    if (argc > 0) path = args[0];

    ejsval mode = _ejs_undefined;
    if (argc > 1) mode = args[1];

    if (!EJSVAL_IS_STRING_TYPE(path)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "path must be a string");
    }

    char* utf8_path = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(ToString(path)));
    int int_mode;
    if (EJSVAL_IS_UNDEFINED(mode))
        int_mode = 0777;
    else if (EJSVAL_IS_STRING_TYPE(mode))
        int_mode = string_to_mode(mode);
    else
        int_mode = ToInteger(mode);

    int rv = mkdir(utf8_path, int_mode);

    if (rv == -1)
        throw_errno_error(errno, utf8_path);


    free(utf8_path);
    return _ejs_undefined;
}

ejsval
_ejs_fs_rmdirSync (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval path = _ejs_undefined;
    if (argc > 0) path = args[0];

    if (!EJSVAL_IS_STRING_TYPE(path)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "path must be a string");
    }

    char* utf8_path = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(ToString(args[0])));

    int rv = rmdir(utf8_path);

    if (rv == -1)
        throw_errno_error(errno, utf8_path);

    free(utf8_path);
    return _ejs_undefined;
}

ejsval
_ejs_fs_readdirSync (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    ejsval path = _ejs_undefined;
    if (argc > 0) path = args[0];

    if (!EJSVAL_IS_STRING_TYPE(path)) {
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "path must be a string");
    }

    char *utf8_path = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(ToString(path)));

    DIR* dir = opendir(utf8_path);
    if (dir == NULL)
        throw_errno_error(errno, utf8_path);

    ejsval array = _ejs_array_new(0, EJS_FALSE);
    struct dirent* entry;

    while ((entry = readdir(dir))) {
        ejsval entry_name = _ejs_string_new_utf8(entry->d_name);
        _ejs_array_push_dense(array, 1, &entry_name);
    }

    free(utf8_path);
    closedir(dir);
    return array;
}

ejsval
_ejs_fs_module_func (ejsval exports)
{
    ejsval internal_fd_descr = _ejs_string_new_utf8("%internal_fd");
    _ejs_gc_add_root (&_ejs_internal_fd_sym);
    _ejs_internal_fd_sym = _ejs_symbol_new(internal_fd_descr);

    EJS_INSTALL_FUNCTION(exports, "statSync", _ejs_fs_statSync);
    EJS_INSTALL_FUNCTION(exports, "existsSync", _ejs_fs_existsSync);
    EJS_INSTALL_FUNCTION(exports, "readFileSync", _ejs_fs_readFileSync);
    EJS_INSTALL_FUNCTION(exports, "writeFileSync", _ejs_fs_writeFileSync);
    EJS_INSTALL_FUNCTION(exports, "createWriteStream", _ejs_fs_createWriteStream);
    EJS_INSTALL_FUNCTION(exports, "mkdirSync", _ejs_fs_mkdirSync);
    EJS_INSTALL_FUNCTION(exports, "rmdirSync", _ejs_fs_mkdirSync);
    EJS_INSTALL_FUNCTION(exports, "readdirSync", _ejs_fs_readdirSync);

    return _ejs_undefined;
}

ejsval
_ejs_os_tmpdir (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
    char *tmpdir = getenv("TMPDIR");

    // XXX i'm sure we need to worry about platform specific tmpdirs (ios?)
    if (!tmpdir) tmpdir = (char*)"/tmp";

    return _ejs_string_new_utf8(tmpdir);
}

ejsval
_ejs_os_arch (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
#if TARGET_CPU_ARM
    const char* arch = "arm";
#elif TARGET_CPU_AMD64
    const char* arch = "x64";
#elif TARGET_CPU_X86
    const char* arch = "x86";
#else
    const char* arch = "unknown";
#endif

    return _ejs_string_new_utf8(arch);
}

ejsval
_ejs_os_platform (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
#if linux
    const char* platform = "linux";
#elif OSX || IOS
    const char* platform = "darwin";
#else
    const char* arch = "unknown";
#endif

    return _ejs_string_new_utf8(platform);
}

ejsval
_ejs_os_module_func (ejsval exports)
{
    EJS_INSTALL_FUNCTION(exports, "tmpdir", _ejs_os_tmpdir);
    EJS_INSTALL_FUNCTION(exports, "arch", _ejs_os_arch);
    EJS_INSTALL_FUNCTION(exports, "platform", _ejs_os_platform);

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
_ejs_child_process_module_func (ejsval exports)
{
    EJS_INSTALL_FUNCTION(exports, "spawn", _ejs_child_process_spawn);

    _ejs_object_setprop_utf8 (exports, "stdout", _ejs_wrapFdWithStream(1));
    _ejs_object_setprop_utf8 (exports, "stderr", _ejs_wrapFdWithStream(2));

    return _ejs_undefined;
}
