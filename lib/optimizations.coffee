esprima = require 'esprima'
escodegen = require 'escodegen'
syntax = esprima.Syntax
debug = require 'debug'

{ NodeVisitor } = require 'nodevisitor'

{ create_intrinsic } = require 'echo-util'

#
# EqIdioms checks for the following things:
#
# typeof() checks against constant strings.
#
#   For most cases we can inline the test directly into LLVM IR (in
#   compiler.coffee), and in the cases where we can't easily, we can
#   call specialized runtime builtins that don't require us to
#   allocate a string do a comparison.
#
# ... more?
# 
class EqIdioms extends NodeVisitor
        is_typeof = (e) -> e.type is syntax.UnaryExpression and e.operator is "typeof"
        is_string_literal = (e) -> e.type is syntax.Literal and typeof e.value is "string"

        visitBinaryExpression: (exp) ->
                return super if exp.operator isnt "==" and exp.operator isnt "===" and exp.operator isnt "!=" and exp.operator isnt "!=="

                left = exp.left
                right = exp.right

                # for typeof checks against string literals, both == and === work
                if (is_typeof(left) and is_string_literal(right)) or (is_typeof(right) and is_string_literal(left))
                        if is_typeof left
                                typecheck = right.value
                                typeofarg = left.argument
                        else
                                typecheck = left.value
                                typeofarg = right.argument

                        switch typecheck
                                when "object"    then intrinsic = "typeofIsObject"
                                when "function"  then intrinsic = "typeofIsFunction"
                                when "string"    then intrinsic = "typeofIsString"
                                when "undefined" then intrinsic = "typeofIsUndefined"
                                when "number"    then intrinsic = "typeofIsNumber"
                                when "null"      then intrinsic = "typeofIsNull"
                                when "boolean"   then intrinsic = "typeofIsBoolean"
                                else
                                        throw new Error "invalid typeof check against '#{typecheck}'";

                        rv = create_intrinsic intrinsic, [typeofarg]
                        if exp.operator[0] is '!'
                                rv = {
                                        type: syntax.UnaryExpression
                                        operator: "!"
                                        argument: rv
                                }
                        return rv

                super

exports.run = (tree) ->

        eq_idioms = new EqIdioms
        tree = eq_idioms.visit tree
        
        return tree
