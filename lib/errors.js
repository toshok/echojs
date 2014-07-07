class SourceError extends Error {
    constructor (errorType, message, filename, loc = { start: { line: -1, column: -2 }}) {
	this.errorType = errorType;
	this.message = message;
	this.filename = filename;
	this.loc = loc;

        super(this.message);
    }

    reportToUser () {
	throw this;
    }
                
    toString () {
        return `${this.filename}:${this.loc.start.line}:${this.loc.start.column+1}: ${this.errorType}: ${this.message}`;
    }
}

const ReportType = {
    error: 0,
    warn: 1
};

function reportToUser (type, errorType, message, filename, loc) {
    if (type === ReportType.error)
	throw new SourceError(errorType.name, message, filename, loc);
    else
        console.warn("${filename}:${loc.start.line}:${loc.start.column+1}: warning: ${message}");
}

export function reportError (errorType, message, filename, loc){
    reportToUser(ReportType.error, errorType, message, filename, loc);
}

export function reportWarning (message, filename, loc) {
    reportToUser(ReportType.warning, null, message, filename, loc);
}
