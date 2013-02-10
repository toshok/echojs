types = require 'types'
llvm = require 'llvm'

exports.jschar = (c) ->
        constant = llvm.Constant.getIntegerValue types.jschar, c
        constant.is_constant = true
        constant.constant_val = c
        constant

exports.int32 = (c) ->
        constant = llvm.Constant.getIntegerValue types.int32, c
        constant.is_constant = true
        constant.constant_val = c
        constant
                
exports.int64 = (c) ->
        constant = llvm.Constant.getIntegerValue types.int64, c
        constant.is_constant = true
        constant.constant_val = c
        constant

exports.null = (t) -> llvm.Constant.getNull t

exports.true = -> llvm.Constant.getIntegerValue types.bool, 1
exports.false = -> llvm.Constant.getIntegerValue types.bool, 0

exports.bool = (c) ->
        constant = llvm.Constant.getIntegerValue types.bool, if c is false then 0 else 1
        constant.is_constant = true
        constant.constant_val = c
        constant

