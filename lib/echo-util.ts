/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

import * as b from "./ast-builder";
import type {
    CallExpression,
    Expression,
    Identifier,
    Node,
    SourceLocation,
    SpreadElement,
} from "./estree";

export function startGenerator(): () => number {
    let _gen = 0;
    return () => {
        const id = _gen;
        _gen += 1;
        return id;
    };
}

const filenameGenerator = startGenerator();

export function genFreshFileName(x: string): string {
    return `${x}.${filenameGenerator()}`;
}

export function bold(): string {
    return "";
}

export function reset(): string {
    return "";
}

export function underline(str: string): string {
    return str + "\n" + "-".repeat(str.length);
}

export function is_string_literal(n: Node): boolean {
    return n.type === b.Literal && typeof n.raw === "string";
}

// a call whose callee is a %-named identifier is a compiler intrinsic
// (a lowering directive minted by the desugar passes, never user code —
// '%' can't appear in a parsed identifier)
export function is_intrinsic(n: Node, name?: string): boolean {
    if (n.type !== b.CallExpression) return false;
    if (n.callee.type !== b.Identifier) return false;
    if (n.callee.name[0] !== "%") return false;
    if (name && n.callee.name !== name) return false;

    return true;
}

export function intrinsic(
    id: Identifier,
    args: (Expression | SpreadElement)[],
    loc?: SourceLocation | null
): CallExpression {
    const rv = b.callExpression(id, args);
    rv.loc = loc;
    return rv;
}

export function sanitize_with_regexp(filename: string): string {
    return filename.replace(/[.,-/\\]/g, "_"); // this is insanely inadequate
}

interface WritableStream {
    write(msg: string): void;
    isTTY?: boolean;
    columns?: number;
}

export class Writer {
    stream: WritableStream;
    have_blank_line = true;

    constructor(stream: WritableStream) {
        this.stream = stream;
    }

    write(msg: string, want_newline = false): void {
        if (want_newline) {
            if (!this.have_blank_line) {
                this.stream.write("\n");
            }
        }
        let out_msg = String(msg);
        if (this.stream.isTTY && this.stream.columns && this.stream.columns > 0) {
            const cols = this.stream.columns;
            if (out_msg.length >= cols) {
                // we should be awesome here and elide something from
                // the middle of the line
                const elide_length = out_msg.length - cols + 5;
                if (elide_length < 0) {
                    // XXX something more here...
                    out_msg = out_msg.substr(0, cols);
                } else {
                    const elide_start = out_msg.length / 2 - elide_length / 2;
                    const elide_end = out_msg.length / 2 + elide_length / 2;

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
