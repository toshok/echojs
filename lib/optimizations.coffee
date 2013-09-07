esprima = require 'esprima'
escodegen = require 'escodegen'
syntax = esprima.Syntax
debug = require 'debug'

{ TreeTransformer } = require 'nodevisitor'

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
class EqIdioms extends TreeTransformer
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

###
# this class looks for loops whose counter is never assigned inside
# the body of the loop, and we can detect that it always remains in
# the range of int32.  this allows us to replace all uses of the loop
# counter by INT32_TO_EJSVAL(), and use an actual machine int32 for
# the loop.
#
# we're super conservative here, and require that the update either
# uses ++, --, _ = _ + _, _ = _ - +, +=, or -= (the latter 4 with an
# integer literal), and the lhs of the update is an identifier
# assigned to a literal in the init.
#
# so for instance, this loop can be optimized:
#   for (var i = 0; i < n; i ++) ...
#
# but this one can't be:
#
#   var i = 0;
#   for (; i < n; i ++) ...
#
# and neither can this one:
#
#   for (i = 0; i < 100000; i += n) ...
# 
class LoopCounter extends TreeTransformer
        visitFor: (exp) ->
                # we only handle update and assignment expressions
                if exp.update.left.type isnt syntax.Identifier
                        return super

                update_identifier_name = exp.update.left.name
                
                if exp.update.type isnt syntax.UpdateExpression and exp.update.type isnt syntax.AssignmentExpression
                        return super

                if exp.update.type is syntax.AssignmentExpression
                        update_right = exp.update.right
                                
                        # of the +=|-=|*=|/= operators, we only handle + and -
                        if exp.update.operator.length is 2
                                if exp.update.operator[0] is '+' or exp.update.operator[0] is '-'
                                        if update_right.type isnt syntax.Literal or (typeof update_right.value) isnt 'number'
                                                return super
                                        # else we win and the update expression is ok
                                else
                                        return super
                        # and for normal assignments, the rhs must be a binary expression with operator of + or - with the same identifier and an integer literal
                        else
                                if update_right.type isnt syntax.BinaryExpression
                                        return super

                                if update_right.operator isnt '+' and update_right.operator isnt '-'
                                        return super

                                if update_right.left.type is syntax.Identifier and update_right.right.type is syntax.Literal
                                        if typeof update_right.right.value isnt 'number'
                                                return super
                                        else if update_right.left.name isnt update_identifier_name
                                                return super
                                        # else we win and the update expression is ok
                                else if update_right.left.type is syntax.Literal and update_right.right.type is syntax.Identifier
                                        if typeof update_right.left.value isnt 'number'
                                                return super
                                        else if update_right.right.name isnt update_identifier_name
                                                return super
                                        # else we win and the update expression is ok
                                else
                                        return super

                else if exp.update.type is syntax.UpdateExpression
                        if update_right.type isnt syntax.Literal or typeof update_right.value isnt 'number'
                                return super
                        # else we win and the update expression is ok
                else
                        return super

                # now make sure the init block initializes update_identifier_name

                if exp.init.type isnt VariableDeclaration and exp.init.type isnt AssignmentExpression
                        return super

                if exp.init.type is VariableDeclaration
                        found = false
                        # check for var/let $update_identifier_name = ...
                        found = found || (decl.id.name is update_identifier_name and decl.init.type is syntax.Literal and typeof decl.init.value is 'number') for decl in exp.init.declarations
                        if not found
                                return super
                        # else we win and the initializer is ok
                else
                        if exp.init.left.type isnt syntax.Identifier or exp.init.left.name isnt update_identifier_name
                                return super
                        # else we win and the initializer is ok
                        
                        
                # if we've made it here, we have an expression we can deal with.
###                
        
exports.run = (tree) ->

        eq_idioms = new EqIdioms
        tree = eq_idioms.visit tree
        
        return tree
