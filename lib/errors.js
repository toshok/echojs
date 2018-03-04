/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

class SourceError extends Error {
    constructor(errorType, message, filename, loc = { start: { line: -1, column: -2 } }) {
        super(message);
        this.errorType = errorType;
        this.message = message;
        this.filename = filename;
        this.loc = loc;
    }

    reportToUser() {
        throw this;
    }

    toString() {
        return `${this.filename}:${this.loc.start.line}:${this.loc.start.column + 1}: ${
            this.errorType
        }: ${this.message}`;
    }
}

const ReportType = {
    error: 0,
    warn: 1,
};

function reportToUser(type, errorType, message, filename, loc) {
    if (type === ReportType.error) throw new SourceError(errorType.name, message, filename, loc);
    else if (loc && loc.start)
        console.warn(`${filename}:${loc.start.line}:${loc.start.column + 1}: warning: ${message}`);
    else console.warn(`${filename}:-1:-1: warning: ${message}`);
}

export function reportError(errorType, message, filename, loc) {
    reportToUser(ReportType.error, errorType, message, filename, loc);
}

export function reportWarning(message, filename, loc) {
    reportToUser(ReportType.warning, null, message, filename, loc);
}
