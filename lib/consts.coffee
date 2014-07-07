types = require 'types'
llvm = require 'llvm'

exports.string = (ir, c) ->
        constant = ir.createGlobalStringPtr c, "strconst"
        constant.is_constant = true
        constant.constant_val = c
        constant
        
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

exports.int1 = (c) ->
        constant = llvm.Constant.getIntegerValue types.int1, c
        constant.is_constant = true
        constant.constant_val = c
        constant
                                
exports.int64 = (c) ->
        constant = llvm.Constant.getIntegerValue types.int64, c
        constant.is_constant = true
        constant.constant_val = c
        constant

exports.int64_lowhi = (ch, cl) ->
        constant = llvm.Constant.getIntegerValue types.int64, ch, cl
        constant.is_constant = true
        constant.constant_val = [ ch, cl ]
        constant

exports.null = (t) -> llvm.Constant.getNull t

exports.true = -> llvm.Constant.getIntegerValue types.bool, 1
exports.false = -> llvm.Constant.getIntegerValue types.bool, 0

exports.bool = (c) ->
        constant = llvm.Constant.getIntegerValue types.bool, if c is false then 0 else 1
        constant.is_constant = true
        constant.constant_val = c
        constant

exports.ejsval_true  = () -> exports.int64_lowhi(0xfff98000, 0x00000001)
exports.ejsval_false = () -> exports.int64_lowhi(0xfff98000, 0x00000000)