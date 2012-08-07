
indent = 0
debug_level = 1

exports.log = (msg, level) ->
  level = level || 0
  if level < debug_level
    return
  str = ""
  str += "  " for i in [0..indent-1]
  str += msg
  console.log str

exports.indent = () -> indent += 1
exports.unindent = () -> indent -= 1
exports.setLevel = (x) -> debug_level = x
