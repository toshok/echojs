terminal = require "terminal"
esprima = require 'esprima'
syntax = esprima.Syntax

b = require 'ast-builder'

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

exports.is_number_literal = (n) -> n.type is syntax.Literal and typeof n.value is "number"
exports.is_string_literal = (n) -> n.type is syntax.Literal and typeof n.raw is "string"
exports.create_intrinsic = (id, args, loc) ->
        type: syntax.CallExpression
        callee: id
        arguments: args
        loc: loc
exports.is_intrinsic = (n, name) ->
        return false if n.type isnt syntax.CallExpression
        return false if n.callee.type isnt syntax.Identifier
        return false if n.callee.name[0] isnt "%"
        if name?
                return false if n.callee.name isnt name
        true

exports.intrinsic = (id, args, loc) ->
        rv = b.callExpression(id, args)
        rv.loc = loc
        rv

