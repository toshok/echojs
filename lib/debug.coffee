
indent = 0
debug_level = 0

exports.log = (msg, level = 1) ->
        if debug_level < level
                return
        if typeof msg isnt "string"
                msg = msg()
        console.warn "#{(' ' for i in [0..indent]).join('')}#{msg}"

exports.indent = () -> indent += 1
exports.unindent = () -> indent -= 1
exports.setLevel = (x) -> debug_level = x
