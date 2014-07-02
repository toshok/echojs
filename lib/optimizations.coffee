esprima = require 'esprima'
escodegen = require 'escodegen'

{ BinaryExpression,
  UnaryExpression,
  Literal,
  AssignmentExpression,
  Identifier,
  UpdateExpression,
  VariableDeclaration } = esprima.Syntax;

debug = require 'debug'
{ TreeVisitor } = require 'nodevisitor'

{ create_intrinsic, is_intrinsic } = require 'echo-util'
b = require 'ast-builder'

typeofIsObject_id = b.identifier "%typeofIsObject"
typeofIsFunction_id = b.identifier "%typeofIsFunction"
typeofIsString_id = b.identifier "%typeofIsString"
typeofIsSymbol_id = b.identifier "%typeofIsSymbol"
typeofIsNumber_id = b.identifier "%typeofIsNumber"
typeofIsBoolean_id = b.identifier "%typeofIsBoolean"
isNull_id = b.identifier "%isNull"
isUndefined_id = b.identifier "%isUndefined"
isNullOrUndefined_id = b.identifier "%isNullOrUndefined"
builtinUndefined_id = b.identifier "%builtinUndefined"

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
# ==/!= of constants null or undefined
# 
class EqIdioms extends TreeVisitor
        is_typeof = (e) -> e.type is UnaryExpression and e.operator is "typeof"
        is_string_literal = (e) -> e.type is Literal and typeof e.value is "string"
        is_undefined_literal = (e) -> e.type is Literal and e.value is undefined
        is_null_literal = (e) -> e.type is Literal and e.value is null

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
                                when "object"    then intrinsic = typeofIsObject_id
                                when "function"  then intrinsic = typeofIsFunction_id
                                when "string"    then intrinsic = typeofIsString_id
                                when "symbol"    then intrinsic = typeofIsSymbol_id
                                when "number"    then intrinsic = typeofIsNumber_id
                                when "boolean"   then intrinsic = typeofIsBoolean_id
                                when "null"      then intrinsic = isNull_id
                                when "undefined" then intrinsic = isUndefined_id
                                else
                                        throw new Error "invalid typeof check against '#{typecheck}'";

                        rv = create_intrinsic intrinsic, [typeofarg]
                        if exp.operator[0] is '!'
                                rv = b.unaryExpression('!', rv)
                        return rv

                if exp.operator is "==" or exp.operator is "!="
                        if is_null_literal(left) or is_null_literal(right) or is_undefined_literal(left) or is_undefined_literal(right)
                                
                                checkarg = if is_null_literal(left) or is_undefined_literal(left) then right else left
                                
                                rv = create_intrinsic isNullOrUndefined_id, [checkarg]
                                if exp.operator is "!="
                                        rv = b.unaryExpression('!', rv)
                                return rv

                if exp.operator is "===" or exp.operator is "!=="
                        if is_null_literal(left) or is_null_literal(right)
                                
                                checkarg = if is_null_literal(left) then right else left

                                rv = create_intrinsic isNull_id, [checkarg]
                                if exp.operator is "!=="
                                        rv = b.unaryExpression('!', rv)
                                return rv
                        if is_undefined_literal(left) or is_undefined_literal(right)
                                
                                checkarg = if is_null_literal(left) then right else left

                                rv = create_intrinsic isNull_id, [checkarg]
                                if exp.operator is "!=="
                                        rv = b.unaryExpression('!', rv)
                                return rv
                super

class ReplaceUnaryVoid extends TreeVisitor
        constructor: -> super
        
        visitUnaryExpression: (n) ->
                if n.operator is "void" and n.argument.type is Literal and n.argument.value is 0
                        return create_intrinsic builtinUndefined_id, []
                n

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
class LoopCounter extends TreeVisitor
        visitFor: (exp) ->
                # we only handle update and assignment expressions
                if exp.update.left.type isnt Identifier
                        return super

                update_identifier_name = exp.update.left.name
                
                if exp.update.type isnt UpdateExpression and exp.update.type isnt AssignmentExpression
                        return super

                if exp.update.type is AssignmentExpression
                        update_right = exp.update.right
                                
                        # of the +=|-=|*=|/= operators, we only handle + and -
                        if exp.update.operator.length is 2
                                if exp.update.operator[0] is '+' or exp.update.operator[0] is '-'
                                        if update_right.type isnt Literal or (typeof update_right.value) isnt 'number'
                                                return super
                                        # else we win and the update expression is ok
                                else
                                        return super
                        # and for normal assignments, the rhs must be a binary expression with operator of + or - with the same identifier and an integer literal
                        else
                                if update_right.type isnt BinaryExpression
                                        return super

                                if update_right.operator isnt '+' and update_right.operator isnt '-'
                                        return super

                                if update_right.left.type is Identifier and update_right.right.type is Literal
                                        if typeof update_right.right.value isnt 'number'
                                                return super
                                        else if update_right.left.name isnt update_identifier_name
                                                return super
                                        # else we win and the update expression is ok
                                else if update_right.left.type is Literal and update_right.right.type is Identifier
                                        if typeof update_right.left.value isnt 'number'
                                                return super
                                        else if update_right.right.name isnt update_identifier_name
                                                return super
                                        # else we win and the update expression is ok
                                else
                                        return super

                else if exp.update.type is UpdateExpression
                        if update_right.type isnt Literal or typeof update_right.value isnt 'number'
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
                        found = found || (decl.id.name is update_identifier_name and decl.init.type is Literal and typeof decl.init.value is 'number') for decl in exp.init.declarations
                        if not found
                                return super
                        # else we win and the initializer is ok
                else
                        if exp.init.left.type isnt Identifier or exp.init.left.name isnt update_identifier_name
                                return super
                        # else we win and the initializer is ok
                        
                        
                # if we've made it here, we have an expression we can deal with.
###                

passes = [
        EqIdioms
        ReplaceUnaryVoid
        ]
        
exports.run = (tree) ->

        passes.forEach (passType) ->
                pass = new passType
                tree = pass.visit tree
                debug.log 2, "after: #{passType.name}"
                debug.log 2, -> escodegen.generate tree

        return tree
