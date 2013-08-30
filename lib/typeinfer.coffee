esprima = require 'esprima'
syntax = esprima.Syntax
escodegen = require 'escodegen'
debug = require 'debug'

{ Stack } = require 'stack'
{ Set } = require 'set'
{ NodeVisitor } = require 'nodevisitor'

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

class InferVisitor extends NodeVisitor
        constructor: (@initial_env) ->
                @stable = false
                super true

        isStable: -> @stable

        visit: (exp) ->
                console.warn "visiting #{exp.type}"
                super
        
        infer: (exp) ->
                while not @stable
                        @stable = true
                        exp = @visit exp
                console.warn "types are stable, returning from inference pass"
                exp

        visitProgram: (program) ->
                super

        visitIdentifier: (exp) ->
                if not InferVisitor.getType exp
                        # check if the identifier is defined in the env
                        if exp.name in @env
                                @assignType exp, @env[exp.name]
                exp

        visitCallExpression: (exp) ->
                super
                # unify the callee type with argtypes
                # this is the complicated bit, since information can travel in both directions:
                #   1. if the function body (and therefore the function type) has an undefined/any type
                exp

        visitFunctionExpression: (exp) ->
                exp

        visitReturn: (n) ->
                debug.log "visitReturn"
                #if @iifeStack.top.iife_rv?

        visitBinaryExpression: (exp) ->
                super
                switch exp.operator
                        when "+"
                                if StringType.is(@getType exp.left) or StringType.is(@getType exp.right)
                                        @assignType exp, new StringType
                                else
                                        @assignType exp, new NumberType
                        when "-"
                                # FIXME we need a way to push a the
                                # NumberType down to left/right so
                                # the compiler can possibly optimize
                                # them.
                                @assignType exp, new NumberType
                        when "*"
                                # FIXME we need a way to push a the
                                # NumberType down to left/right so
                                # the compiler can possibly optimize
                                # them.
                                @assignType exp, new NumberType
                        when "/"
                                # FIXME we need a way to push a the
                                # NumberType down to left/right so
                                # the compiler can possibly optimize
                                # them.
                                @assignType exp, new NumberType
                exp
        visitVariableDeclarator: (exp) ->
                super
                # we need to associate the type of exp.init (if there
                # is one) with exp.id in the current environment.
                exp

        visitLiteral: (exp) ->
                if typeof exp.value is 'number'
                        if not NumberType.is InferVisitor.getType exp
                                @assignType exp, new NumberType
                exp

        @getType: (exp) -> return exp._ejs_inferredtype
        getType: (exp) -> InferVisitor.getType exp

        assignType: (ast, ty) ->
                console.warn "assigning type of #{ty}"
                if not ast._ejs_inferredtype
                        # The ast node didn't have a type specified before
                        Object.defineProperty ast, "_ejs_inferredtype",
                                value: ty
                                enumerable: true
                                configurable: true
                                writable: true
                        console.warn "assigned type #{ast._ejs_inferredtype}"
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

class DumpInferredTypesVisitor extends NodeVisitor
        constructor: ->
                super true
                
        visit: (exp) ->
                return null if not exp?
                console.warn "dumping #{exp.type}"
                ty = InferVisitor.getType exp
                if ty
                        console.warn "inferred type of #{escodegen.generate exp} is '#{ty}'"
                super
                        
dump = (tree)->
        dumpvisitor = new DumpInferredTypesVisitor
        dumpvisitor.visit tree
###
dump run esprima.parse "5;"
dump run esprima.parse "4 + 5;"
dump run esprima.parse "function () { return 5; }"
###