
exports.SourceReferenceError = class SourceReferenceError
        constructor: (@message, @filename, @loc) ->

        toString: () ->
                "#{@filename}:#{@loc.start.line}:#{@loc.start.column}: Error #{@message}"

