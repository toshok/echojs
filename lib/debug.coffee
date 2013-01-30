
indent = 0
debug_level = 0

exports.log = () ->
        level = 3
        msg = null

        if arguments.length > 1
                level = arguments[0]
                msg = arguments[1]
        else if arguments.length == 1
                msg = arguments[0]
        
        if debug_level < level
                return
        if typeof msg is "function"
                msg = msg()
        if msg?
                console.warn "#{(' ' for i in [0..indent]).join('')}#{msg}"

exports.indent = () -> indent += 1
exports.unindent = () -> indent -= 1
exports.setLevel = (x) -> debug_level = x
