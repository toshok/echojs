esprima = require 'esprima'
syntax = esprima.Syntax
escodegen = require 'escodegen'
debug = require 'debug'

{ Stack } = require 'stack'
{ Set } = require 'set'
{ TreeVisitor } = require 'nodevisitor'

echo_util = require 'echo-util'

class Type
        isCompatible: (ty) -> false

class FunctionType extends Type
        constructor: (@return_type, @parameter_types) ->

        isCompatible: (ty) ->
                if not ty instanceof FunctionType
                        false
                else if not @return_type.isCompatible ty.return_type
                        false
                else if not parametersCompatible(ty)
                        false
                else
                        true

        parametersCompatible: (param_tys) ->
                # XXX JS allows partial application (leaves the other types Undefined...)
                if param_tys.length != parameter_types.length
                        return false;
                return false if not @parameter_types[i].isCompatible param_tys.length[i] for i in [0..param_tys.length]
                true

        toString: -> "(#{t for t in (@parameter_types.join(', ').split(' '))}) -> #{@return_type}"


class UndefinedType extends Type
        isCompatible: (ty) -> false
        toString: -> "Undefined"
        @is: (ty) -> ty instance of UndefinedType

class VoidType extends Type
        isCompatible: (ty) -> ty instanceof VoidType
        toString: -> "Void"
        @is: (ty) -> ty instanceof VoidType

class AnyType extends Type
        isCompatible: (ty) -> true
        toString: -> "Any"
        @is: (ty) -> ty instanceof AnyType

class NumberType extends Type
        isCompatible: (ty) -> ty instanceof NumberType
        toString: -> "Number"
        @is: (ty) -> ty instanceof NumberType

class StringType extends Type
        isCompatible: (ty) -> ty instanceof StringType
        toString: -> "String"
        @is: (ty) -> ty instanceof StringType

class MultiType extends Type
        constructor: ->
                @types = []

        isCompatible: (ty) ->
                for mty in @types
                        if mty isCompatible ty
                                true
                false

        addType: (ty) ->
                @types.unshift ty

        toString: -> "Multi[#{t for t in @types}]"

class InferVisitor extends TreeVisitor
        constructor: (@initial_env) ->
                @stable = false
                @function_stack = new Stack

        isStable: -> @stable

        visit: (exp) ->
                super
        
        infer: (exp) ->
                while not @stable
                        @stable = true
                        exp = @visit exp
                console.warn "types are stable, returning from inference pass"
                exp

        visitProgram: (program) -> super

        visitIdentifier: (exp) ->
                if not @getType exp
                        # check if the identifier is defined in the env
                        if exp.name in @env
                                @assignType exp, @env[exp.name]
                exp

        visitCallExpression: (exp) ->
                super
                # If we have a type for the callee in our environment,
                # try to look up a matching signature for
                # parameters/arguments.
                # 
                # If we find one, then use the return type of that
                # signature as exp's inferred type.
                #
                # If we don't find one, we need to re-traverse the
                # function using the inferred argument types to see
                # what the return type could be.
                exp

        visitFunctionBody: (exp) ->
                @function_stack.push exp
                exp.body = @visit exp.body
                @function_stack.pop()
                exp

        visitFunctionExpression: (exp) ->
                @visitFunctionBody exp

        visitFunctionDeclaration: (exp) ->
                @visitFunctionBody exp

        visitReturn: (exp) ->
                super
                debug.log "visitReturn"
                @assignType @function_stack.top, new FunctionType (@getType exp.argument), []
                exp

        visitBinaryExpression: (exp) ->
                super
                switch exp.operator
                        when "+"
                                if StringType.is @getType exp.left
                                        @assignType exp, new StringType
                                        @setDesiredType exp.right, new StringType
                                else if StringType.is @getType exp.right
                                        @assignType exp, new StringType
                                        @setDesiredType exp.left, new StringType
                                else
                                        @assignType exp, new NumberType
                                        @setDesiredType exp.left, new NumberType
                                        @setDesiredType exp.right, new NumberType
                        when "-"
                                @assignType exp, new NumberType
                                @setDesiredType exp.left, new NumberType
                                @setDesiredType exp.right, new NumberType
                        when "*"
                                @assignType exp, new NumberType
                                @setDesiredType exp.left, new NumberType
                                @setDesiredType exp.right, new NumberType
                        when "/"
                                @assignType exp, new NumberType
                                @setDesiredType exp.left, new NumberType
                                @setDesiredType exp.right, new NumberType
                exp
        visitVariableDeclarator: (exp) ->
                super
                # we need to associate the type of exp.init (if there
                # is one) with exp.id in the current environment.
                exp

        visitLiteral: (exp) ->
                if typeof exp.value is 'number'
                        @assignType exp, new NumberType
                else if typeof exp.value is 'string'
                        @assignType exp, new StringType
                exp

        @getType: (exp) -> return exp._ejs_inferredtype
        getType: (exp) -> InferVisitor.getType exp

        assignType: (ast, ty) ->
                #console.warn "assigning type of #{ty}"
                if not ast._ejs_inferredtype
                        # The ast node didn't have a type specified before
                        Object.defineProperty ast, "_ejs_inferredtype",
                                value: ty
                                enumerable: true
                                configurable: true
                                writable: true
                        #console.warn "assigned type #{ast._ejs_inferredtype}"
                else if ast._ejs_inferredtype instanceof MultiType
                        # The ast node is already polymorphic, add the new type to the bunch
                        ast._ejs_inferredtype.addType ty
                else
                        # The ast node is monomorphic, and now becomes polymorphic
                        current_type = ast._ejs_inferredtype
                        ast._ejs_inferredtype = new MultiType
                        ast._ejs_inferredtype.addType current_type
                        ast._ejs_inferredtype.addType ty

                @changed = true

        setDesiredType: (ast, ty) ->
                #console.warn "setting desired type of #{ty}"
                ast._ejs_desiredtype = ty
                @changed = true

class TypeInfer
        constructor: ->
                @defaultGlobalEnvironment = [
                        { "print": (new FunctionType (new VoidType()), [new AnyType()]) }
                ]

        run: (tree) ->
                # walk the entire tree placing UndefinedTypes everywhere.
                # placeUndefinedTypes @tree

                visitor = new InferVisitor @defaultGlobalEnvironment
                visitor.infer tree

exports.run = run = (tree) ->
        typeinfer = new TypeInfer
        typeinfer.run tree

# tests

class DumpInferredTypesVisitor extends TreeVisitor
        visit: (exp) ->
                return null if not exp?
                #console.warn "dumping #{exp.type}"
                ty = InferVisitor.getType exp
                if ty
                        console.warn "inferred type of #{escodegen.generate exp} is '#{ty}'"
                super
                        
dump = (tree)->
        dumpvisitor = new DumpInferredTypesVisitor
        dumpvisitor.visit tree

dump run esprima.parse "5;"
dump run esprima.parse "4 + 5;"
dump run esprima.parse "function foo () { return 5; }"
dump run esprima.parse "function foo () { return 5; return 'hi'; }"
