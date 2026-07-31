/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

type Message = string | (() => string);

let _indent = 0;
let _debug_level = 0;

export function log(msg: Message): void;
export function log(level: number, msg: Message): void;
export function log(levelOrMsg: number | Message, maybeMsg?: Message): void {
    let level: number;
    let msg: Message;

    if (maybeMsg !== undefined) {
        level = levelOrMsg as number;
        msg = maybeMsg;
    } else {
        level = 3;
        msg = levelOrMsg as Message;
    }

    if (_debug_level < level) return;

    const text = typeof msg === "function" ? msg() : msg;

    if (text) console.warn(text);
}

export function indent(): void {
    _indent += 1;
}

export function unindent(): void {
    _indent -= 1;
    if (_indent < 0) {
        console.warn("indent level mismatch.  setting to 0");
        _indent = 0;
    }
}

export function setLevel(x: number): void {
    _debug_level = x;
}

export function time(level: number, id: string): void {
    if (_debug_level < level) return;
    console.time(id);
}

export function timeEnd(level: number, id: string): void {
    if (_debug_level < level) return;
    console.timeEnd(id);
}
