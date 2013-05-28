terminal = require "./terminal.js"

exports.deep_copy_object = (o) -> JSON.parse JSON.stringify o

exports.map = map = (f, arr) ->
        f el for el in arr

exports.foldl = foldl = (f, z, arr) ->
        return z if arr.length is 0
        return foldl f, (f z, arr[0]), (arr.slice 1)

exports.reject = (o, pred) ->
        rv = Object.create null
        rv[prop] = o[prop] for prop in (Object.getOwnPropertyNames o) when not pred prop
        rv
        
gen = 0

exports.genId = genId = ->
        id = gen
        gen += 1
        id
        
exports.genFreshFileName = (x) ->
        "#{x}.#{genId()}"

exports.genGlobalFunctionName = (x, filename) ->
        prefix = if filename? then "__ejs[#{filename}]" else "__ejs_fn"
        "#{prefix}_#{x}_#{genId()}"

exports.genAnonymousFunctionName = (filename) ->
        prefix = if filename? then "__ejs[#{filename}]_%anon" else "__ejs_%anon"
        "#{prefix}_#{genId()}"

exports.bold = ->
        if process?.stderr?.isTTY
                return terminal.ANSIStyle("bold");
        return ""
                
exports.reset = ->
        if process?.stderr?.isTTY
                return terminal.ANSIStyle("reset");
        return ""
