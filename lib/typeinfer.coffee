lexer = require './lexer'
parser = require './parser'
definitions = require './definitions'
tokens = definitions.tokens

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


class UndefinedType extends Type
  isCompatible: (ty) -> false
  @is: (ty) -> ty instance of UndefinedType

class VoidType extends Type
  isCompatible: (ty) -> ty instanceof VoidType
  @is: (ty) -> ty instanceof VoidType

class AnyType extends Type
  isCompatible: (ty) -> true
  @is: (ty) -> ty instanceof AnyType

class NumberType extends Type
  isCompatible: (ty) -> ty instanceof NumberType
  @is: (ty) -> ty instanceof NumberType

class StringType extends Type
  isCompatible: (ty) -> ty instanceof StringType
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

class InferVisitor
  constructor: ->
    @changed = false
  
  infer: (ast,env) ->
    @changed = false
    @inferAst ast

  isChanged: -> @changed


  inferAst: (ast,env) ->
    if ast instanceof parser.Script
      # scripts are an odd case.  they don't have a type themselves, but contain things that do,
      # so we just iterate over it
      @inferAst node for node in ast.children
    else if ast instanceof parser.Number
      if not NumberType.is(ast.type)
        assignType ast, new NumberType
    else if ast instanceof parser.Identifier
      if not ast.type
        # check if the identifier is defined in the env
        if ast.name in env
          assignType ast, env[ast.name]
    else if ast instanceof parser.Decl
      if ast.initializer
        env[ast.name] = @inferAst ast.initializer, env
    else if ast instanceof parser.Call
      funtype = @infertAst ast.callee, env
      argtypes = @inferAst arg, env for arg in ast.arguments
      # unify the funtype with argtypes
      # this is the complicated bit, since information can travel in both directions:
      #   1. if the function body (and therefore the function type) has an undefined/any type

    # and finally we return the type of the ast
    ast.type
      
  assignType: (ast, ty) ->
    if not ast.type
      # The ast node didn't have a type specified before
      ast.type = ty
    else if ast.type instanceof MultiType
      # The ast node is already polymorphic, add the new type to the bunch
      ast.type.addType ty
    else
      # The ast node is monomorphic, and now becomes polymorphic
      current_type = ast.type
      ast.type = new MultiType
      ast.type.addType current_type
      ast.type.addType ty

    @changed = true

class TypeInfer
  constructor: (@ast) ->
    @defaultGlobalEnvironment = [
      { "print": (new FunctionType (new VoidType()), [new AnyType()]) }
    ]

#  placeUndefinedTypes: (tree) ->
#    tree.type = new UndefinedType
#    placeUndefinedTypes x for x in tree.children

  run: ->
    # walk the entire tree placing UndefinedTypes everywhere.
    # placeUndefinedTypes @ast

    visitor = new InferVisitor

    visitor.infer @ast, @defaultGlobalEnvironment
    visitor.infer @ast, @defaultGlobalEnvironment while visitor.isChanged()

exports.TypeInfer = TypeInfer
