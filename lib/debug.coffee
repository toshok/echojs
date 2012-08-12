
indent = 0
debug_level = 0

exports.log = (msg, level = 1) ->
        console.warn "#{(' ' for i in [0..indent]).join('')}#{msg}" if debug_level >= level

exports.indent = () -> indent += 1
exports.unindent = () -> indent -= 1
exports.setLevel = (x) -> debug_level = x
