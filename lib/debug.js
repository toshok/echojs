
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
        console.warn(`${' '.repeat(indent)}${msg}`);
}

export function indent() { indent += 1; }
export function unindent() { indent -= 1; }
export function setLevel(x) { debug_level = x; }
