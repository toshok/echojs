/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */
//let terminal = require('terminal');

import * as b from "./ast-builder";

export function shallow_copy_object(o) {
    if (!o) return null;

    let new_o = Object.create(Object.getPrototypeOf(o));
    for (let x of Object.getOwnPropertyNames(o)) new_o[x] = o[x];
    return new_o;
}

export function map(f, seq) {
    let rv = [];
    for (let el of seq) {
        rv.push(f(el));
    }
    return rv;
}

export function foldl(f, z, arr) {
    if (arr.length === 0) return z;

    return foldl(f, f(z, arr[0]), arr.slice(1));
}

export function reject(o, pred) {
    let rv = Object.create(null);
    for (let prop of Object.getOwnPropertyNames(o)) {
        if (!pred(prop)) rv[prop] = o[prop];
    }
    return rv;
}

export function startGenerator() {
    let _gen = 0;
    return () => {
        let id = _gen;
        _gen += 1;
        return id;
    };
}

let filenameGenerator = startGenerator();

export function genFreshFileName(x) {
    return `${x}.${filenameGenerator()}`;
}

let functionNameGenerator = startGenerator();

export function genGlobalFunctionName(x, filename) {
    let prefix = filename ? `__ejs[${filename}]` : "__ejs_fn";
    return `${prefix}_${x}_${functionNameGenerator()}`;
}

export function genAnonymousFunctionName(filename) {
    let prefix = filename ? `__ejs[${filename}]_%anon` : "__ejs_%anon";
    return `${prefix}_${functionNameGenerator()}`;
}

export function bold() {
    /*
     if (process && process.stderr && process.stderr.isTTY)
     return terminal.ANSIStyle('bold');
     */
    return "";
}

export function reset() {
    /*
     if (process && process.stderr && process.stderr.isTTY)
     return terminal.ANSIStyle('reset');
     */
    return "";
}

export function underline(str) {
    return str + "\n" + "-".repeat(str.length);
}

export function is_number_literal(n) {
    return n.type === b.Literal && typeof n.value === "number";
}
export function is_string_literal(n) {
    return n.type === b.Literal && typeof n.raw === "string";
}
export function create_intrinsic(id, args, loc) {
    return {
        type: b.CallExpression,
        callee: id,
        arguments: args,
        loc: loc,
    };
}

export function is_intrinsic(n, name) {
    if (n.type !== b.CallExpression) return false;
    if (n.callee.type !== b.Identifier) return false;
    if (n.callee.name[0] !== "%") return false;
    if (name && n.callee.name !== name) return false;

    return true;
}

export function intrinsic(id, args, loc) {
    let rv = b.callExpression(id, args);
    rv.loc = loc;
    return rv;
}

export function sanitize_with_regexp(filename) {
    return filename.replace(/[.,-\/\\]/g, "_"); // this is insanely inadequate
}

export class Writer {
    constructor(stream) {
        this.stream = stream;
        this.have_blank_line = true;
    }

    write(msg, want_newline = false) {
        if (want_newline) {
            if (!this.have_blank_line) {
                this.stream.write("\n");
            }
        }
        let out_msg = String(msg);
        if (this.stream.isTTY && this.stream.columns > 0) {
            let cols = this.stream.columns;
            if (out_msg.length >= cols) {
                // we should be awesome here and elide something from
                // the middle of the line
                let elide_length = out_msg.length - cols + 5;
                if (elide_length < 0) {
                    // XXX something more here...
                    out_msg = out_msg.substr(0, cols);
                } else {
                    let elide_start = out_msg.length / 2 - elide_length / 2;
                    let elide_end = out_msg.length / 2 + elide_length / 2;

                    out_msg = out_msg.slice(0, elide_start) + " ... " + out_msg.slice(elide_end);
                }
            } else {
                out_msg = out_msg + " ".repeat(cols - out_msg.length);
            }
            this.stream.write("\r");
            this.stream.write(out_msg);
            this.have_blank_line = false;
        } else {
            this.stream.write(out_msg + "\n");
        }
    }
}
