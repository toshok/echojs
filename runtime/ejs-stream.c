/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "ejs.h"
#include "ejs-error.h"
#include "ejs-function.h"
#include "ejs-ops.h"
#include "ejs-string.h"
#include "ejs-symbol.h"
#include "ejs-value.h"

static ejsval _ejs_internal_fd_sym EJSVAL_ALIGNMENT; 

static EJS_NATIVE_FUNC(_ejs_stream_write) {
    ejsval to_write = ToString(args[0]);
    ejsval internal_fd = _ejs_object_getprop (*_this, _ejs_internal_fd_sym);
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

static EJS_NATIVE_FUNC(_ejs_stream_end) {
    ejsval internal_fd = _ejs_object_getprop (*_this, _ejs_internal_fd_sym);
    close (ToInteger(internal_fd));
    return _ejs_undefined;
}

static EJS_NATIVE_FUNC(_ejs_stream_endThrow) {
    _ejs_throw_nativeerror_utf8 (EJS_ERROR, "this stream cannot be closed");
    EJS_NOT_REACHED();
}

ejsval
_ejs_stream_wrapFd (int fd, EJSBool throwOnEnd)
{
    ejsval stream = _ejs_object_create(_ejs_null);

    EJS_INSTALL_ATOM_FUNCTION(stream, write, _ejs_stream_write);
    if (throwOnEnd) {
        EJS_INSTALL_ATOM_FUNCTION(stream, end, _ejs_stream_endThrow);
    }
    else {
        EJS_INSTALL_ATOM_FUNCTION(stream, end, _ejs_stream_end);
    }

    _ejs_object_setprop (stream, _ejs_internal_fd_sym, NUMBER_TO_EJSVAL(fd));

    return stream;
}

static EJS_NATIVE_FUNC(_ejs_stream_getColumns) {
    ejsval internal_fd = _ejs_object_getprop (*_this, _ejs_internal_fd_sym);
    int fd = ToInteger(internal_fd);

    struct winsize size;
    if ((ioctl(fd, TIOCGWINSZ, &size) == 0) && size.ws_col) {
        return NUMBER_TO_EJSVAL(size.ws_col);
    }
    return _ejs_undefined;
}


ejsval
_ejs_stream_wrapSysOutput(int fd) {
    ejsval stream = _ejs_stream_wrapFd(fd, EJS_TRUE);
    if (isatty(fd)) {
        _ejs_object_setprop (stream, _ejs_atom_isTTY, _ejs_true);
        EJS_INSTALL_ATOM_GETTER(stream, columns, _ejs_stream_getColumns);
    }
    return stream;
}

void
_ejs_stream_init(ejsval global)
{
    ejsval internal_fd_descr = _ejs_string_new_utf8("%internal_fd");
    _ejs_gc_add_root (&_ejs_internal_fd_sym);
    _ejs_internal_fd_sym = _ejs_symbol_new(internal_fd_descr);
}
