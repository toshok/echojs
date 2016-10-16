/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

let _indent = 0;
let _debug_level = 0;

export function log() {
    let level = 3;
    let msg = null;

    if (arguments.length > 1) {
        level = arguments[0];
        msg = arguments[1];
    }
    else if (arguments.length == 1) {
        msg = arguments[0];
    }

    if (_debug_level < level) return;

    if (typeof msg === 'function')
        msg = msg();

    if (msg)
        //console.warn(`${' '.repeat(_indent)}${msg}`);
        console.warn(msg);
}

export function indent() { _indent += 1; }
export function unindent() { _indent -= 1; if (_indent < 0) { console.warn ('indent level mismatch.  setting to 0'); _indent = 0; } }
export function setLevel(x) { _debug_level = x; }

export function time(level, id) {
    if (_debug_level < level) return;
    console.time(id);
}

export function timeEnd(level, id) {
    if (_debug_level < level) return;
    console.timeEnd(id);
}
