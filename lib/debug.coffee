
indent = 0
debug_level = 0

exports.log = (msg, level = 9) ->
  return
  if level <= debug_level
    return
  str = ""
  str += "  " for i in [0..indent-1]
  str += msg
  console.warn str

exports.indent = () -> indent += 1
exports.unindent = () -> indent -= 1
exports.setLevel = (x) -> debug_level = x
