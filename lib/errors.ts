/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

import type { SourceLocation } from "./estree";

// callers pass an Error subclass constructor (TypeError, ReferenceError,
// ...) whose name labels the diagnostic
export type ErrorType = { name: string };

class SourceError extends Error {
    errorType: string;
    filename: string;
    loc: SourceLocation;

    constructor(
        errorType: string,
        message: string,
        filename: string,
        loc: SourceLocation = { start: { line: -1, column: -2 } }
    ) {
        super(message);
        this.errorType = errorType;
        this.message = message;
        this.filename = filename;
        this.loc = loc;
    }

    reportToUser(): never {
        throw this;
    }

    override toString(): string {
        return `${this.filename}:${this.loc.start.line}:${this.loc.start.column + 1}: ${
            this.errorType
        }: ${this.message}`;
    }
}

export function reportError(
    errorType: ErrorType,
    message: string,
    filename: string,
    loc?: SourceLocation
): never {
    throw new SourceError(errorType.name, message, filename, loc);
}

export function reportWarning(message: string, filename: string, loc?: SourceLocation): void {
    if (loc && loc.start)
        console.warn(`${filename}:${loc.start.line}:${loc.start.column + 1}: warning: ${message}`);
    else console.warn(`${filename}:-1:-1: warning: ${message}`);
}
