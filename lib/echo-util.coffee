
exports.deep_copy_object = (o) -> JSON.parse JSON.stringify o

exports.map = map = (f, arr) ->
        f el for el in arr

exports.foldl = foldl = (f, z, arr) ->
        return z if arr.length is 0
        return foldl f, (f z, arr[0]), (arr.slice 1)

gen = 0
exports.genFreshFileName = (x) ->
        name = "#{x}.#{gen}"
        gen += 1
        name

exports.genGlobalFunctionName = (x, filename) ->
        prefix = if filename? then "__ejs[#{filename}]" else "__ejs_fn"
        name =  "#{prefix}_#{x}_#{gen}"
        gen += 1
        name

exports.genAnonymousFunctionName = (filename) ->
        prefix = if filename? then "__ejs[#{filename}]_%anon" else "__ejs_%anon"
        name =  "#{prefix}_#{gen}"
        gen += 1
        name
        
