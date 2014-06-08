class SourceError extends Error
        constructor: (@errorType, @message, @filename, @loc) ->
                super @message

        reportToUser: () -> throw @
                
        toString: () ->
                "#{@filename}:#{@loc.start.line}:#{@loc.start.column+1}: #{@errorType}: #{@message}"

ReportType =
        error: 0
        warn: 1

reportToUser = (type, errorType, message, filename, loc) ->
        if type is ReportType.error
                throw new SourceError(errorType.name, message, filename, loc)
        else
                console.warn "#{filename}:#{loc.start.line}:#{loc.start.column+1}: warning: #{message}"

reportError = (errorType, message, filename, loc) ->
        reportToUser ReportType.error, errorType, message, filename, loc

reportWarning = (message, filename, loc) ->
        reportToUser ReportType.warning, null, message, filename, loc

exports.reportError = reportError
exports.reportWarning = reportWarning
