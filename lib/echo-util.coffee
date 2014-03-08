terminal = require "terminal"
esprima = require 'esprima'
syntax = esprima.Syntax

exports.shallow_copy_object = (o) ->
        return null if not o?

        new_o = Object.create Object.getPrototypeOf o
        new_o[x] = o[x] for x in Object.getOwnPropertyNames o
        new_o
        
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
        
exports.startGenerator = startGenerator = ->
        _gen = 0
        () ->
                id = _gen
                _gen += 1
                id
                
filenameGenerator = startGenerator()

exports.genFreshFileName = (x) ->
        "#{x}.#{filenameGenerator()}"

functionNameGenerator = startGenerator()

exports.genGlobalFunctionName = (x, filename) ->
        prefix = if filename? then "__ejs[#{filename}]" else "__ejs_fn"
        "#{prefix}_#{x}_#{functionNameGenerator()}"

exports.genAnonymousFunctionName = (filename) ->
        prefix = if filename? then "__ejs[#{filename}]_%anon" else "__ejs_%anon"
        "#{prefix}_#{functionNameGenerator()}"

exports.bold = ->
        if process?.stderr?.isTTY
                return terminal.ANSIStyle("bold");
        return ""
                
exports.reset = ->
        if process?.stderr?.isTTY
                return terminal.ANSIStyle("reset");
        return ""

exports.create_identifier = create_identifier = (x) ->
        throw new Error "invalid name in create_identifier" if not x
        type: syntax.Identifier, name: x
exports.create_string_literal = (x) ->
        throw new Error "invalid string in create_string_literal" if not x
        type: syntax.Literal, value: x, raw: "\"#{x}\""
exports.create_number_literal = (x) ->
        throw new Error "invalid number '#{x}' (#{typeof x}) in create_number_literal" if typeof x isnt "number"
        type: syntax.Literal, value: x, raw: "#{x}"
exports.is_number_literal = (n) ->
        n.type is syntax.Literal and typeof n.value is "number"
exports.create_intrinsic = (id, args) ->
        type: syntax.CallExpression
        callee: id
        arguments: args
exports.is_intrinsic = (name, n) ->
        n.type is syntax.CallExpression and n.callee.name is name

