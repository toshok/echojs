/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

let indent = 0;
let debug_level = 0;

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

    if (debug_level < level) return;

    if (typeof msg === "function")
        msg = msg();

    if (msg)
        //console.warn(`${' '.repeat(indent)}${msg}`);
        console.warn(msg);
}

export function indent() { indent += 1; }
export function unindent() { indent -= 1; if (indent < 0) { console.warn ("indent level mismatch.  setting to 0"); indent = 0; } }
export function setLevel(x) { debug_level = x; }

export function time(level, id) {
    if (debug_level < level) return;
    console.time(id);
}

export function timeEnd(level, id) {
    if (debug_level < level) return;
    console.timeEnd(id);
}
