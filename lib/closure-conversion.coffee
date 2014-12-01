esprima = require 'esprima'
escodegen = require 'escodegen'
debug = require 'debug'

b = require 'ast-builder'

new_cc = require('new-cc')

runtime_globals = require('runtime').createGlobalsInterface(null)

{ reportError, reportWarning } = require 'errors'

#{ CFA2 } = require 'jscfa2'

{ ArrayExpression,
  ArrayPattern,
  ArrowFunctionExpression,
  AssignmentExpression,
  BinaryExpression,
  BlockStatement,
  BreakStatement,
  CallExpression,
  CatchClause,
  ClassBody,
  ClassDeclaration,
  ClassExpression,
  ClassHeritage,
  ComprehensionBlock,
  ComprehensionExpression,
  ConditionalExpression,
  ContinueStatement,
  ComputedPropertyKey,
  DebuggerStatement,
  DoWhileStatement,
  EmptyStatement,
  ExportDeclaration,
  ExportBatchSpecifier,
  ExportSpecifier,
  ExpressionStatement,
  ForInStatement,
  ForOfStatement,
  ForStatement,
  FunctionDeclaration,
  FunctionExpression,
  Identifier,
  IfStatement,
  ImportDeclaration,
  ImportSpecifier,
  LabeledStatement,
  Literal,
  LogicalExpression,
  MemberExpression,
  MethodDefinition,
  ModuleDeclaration,
  NewExpression,
  ObjectExpression,
  ObjectPattern,
  Program,
  Property,
  ReturnStatement,
  SequenceExpression,
  SpreadElement,
  SwitchCase,
  SwitchStatement,
  TaggedTemplateExpression,
  TemplateElement,
  TemplateLiteral,
  ThisExpression,
  ThrowStatement,
  TryStatement,
  UnaryExpression,
  UpdateExpression,
  VariableDeclaration,
  VariableDeclarator,
  WhileStatement,
  WithStatement,
  YieldExpression } = esprima.Syntax

{ Stack } = require 'stack'
{ Set } = require 'set'
{ Map } = require 'map'
{ TreeVisitor } = require 'nodevisitor'

{ genGlobalFunctionName,
  genAnonymousFunctionName,
  shallow_copy_object,
  deep_copy_object,
  map,
  foldl,
  reject,
  is_intrinsic,
  intrinsic,
  startGenerator } = require 'echo-util'

hasOwnProperty = Object.prototype.hasOwnProperty

superid                 = b.identifier "%super"
makeClosureEnv_id       = b.identifier "%makeClosureEnv"
makeClosureNoEnv_id     = b.identifier "%makeClosureNoEnv"
makeClosure_id          = b.identifier "%makeClosure"
makeAnonClosure_id      = b.identifier "%makeAnonClosure"
setSlot_id              = b.identifier "%setSlot"
slot_id                 = b.identifier "%slot"
invokeClosure_id        = b.identifier "%invokeClosure"
getLocal_id             = b.identifier "%getLocal"
getArg_id               = b.identifier "%getArg"
getArgumentsObject_id   = b.identifier "%getArgumentsObject"
moduleGetSlot_id        = b.identifier "%moduleGetSlot"
moduleSetSlot_id        = b.identifier "%moduleSetSlot"
moduleGetExotic_id      = b.identifier "%moduleGetExotic"
templateCallsite_id     = b.identifier "%templateCallsite"
templateHandlerCall_id  = b.identifier "%templateHandlerCall"
templateDefaultHandlerCall_id = b.identifier "%templateDefaultHandlerCall"
createArgScratchArea_id       = b.identifier "%createArgScratchArea"
setPrototypeOf_id       = b.identifier "%setPrototypeOf"
objectCreate_id         = b.identifier "%objectCreate"
gatherRest_id           = b.identifier "%gatherRest"
arrayFromSpread_id      = b.identifier "%arrayFromSpread"
argPresent_id           = b.identifier "%argPresent"

class TransformPass extends TreeVisitor
        constructor: (@options) ->
                
#
# converts:
#
# class Subclass extends Baseclass
#   constructor (/* ctor args */) { /* ctor body */ }
#
#   method (/* method args */) { /* method body */ }
#
# to:
#
# let Subclass = (function(%super) {
#   %extends(Subclass, %super);
#   function Subclass (/* ctor args */) { /* ctor body */ }
#   Subclass.prototype.method = function(/* method args */) { /* method body */ };
#   return Subclass;
# })(Baseclass)
# 
# 
DesugarClasses = class DesugarClasses extends TransformPass
        constructor: ->
                super
                @class_stack = new Stack
                @method_stack = new Stack

        createSuperReference = (is_static, id) ->
                return superid if id?.name is "constructor"

                obj = if is_static then superid else b.memberExpression(superid, b.identifier('prototype'))

                return obj if not id?
                        
                b.memberExpression(obj, id)
        
        visitCallExpression: (n) ->
                if n.callee.type is Identifier and n.callee.name is "super"
                        super_ref = createSuperReference @method_stack.top.static, @method_stack.top.key
                        n.callee = b.memberExpression(super_ref, b.identifier('call'))
                        n.arguments.unshift(b.thisExpression())
                else
                        n.callee = @visit n.callee
                n.arguments = @visitArray n.arguments
                n

        visitNewExpression: (n) ->
                n.callee = @visit n.callee
                n.arguments = @visitArray n.arguments
                n
                
        visitIdentifier: (n) ->
                if n.name is "super"
                        return createSuperReference @method_stack.top.static
                n

        visitClassDeclaration: (n) ->
                iife = @generateClassIIFE(n);
                b.letDeclaration(n.id, b.callExpression(iife, if n.superClass? then [n.superClass] else []))
                
        visitClassExpression: (n) ->
                iife = @generateClassIIFE(n);
                b.callExpression(iife, if n.superClass? then [n.superClass] else [])


        generateClassIIFE: (n) ->
                # we visit all the functions defined in the class so that 'super' is replaced with '%super'
                @class_stack.push n

                # XXX this push/pop should really be handled in @visitMethodDefinition
                for class_element in n.body.body
                        @method_stack.push class_element
                        class_element.value = @visit class_element.value
                        @method_stack.pop()
                
                @class_stack.pop()
                        
                class_init_iife_body = []

                [properties, methods, sproperties, smethods] = @gather_members n

                class_init_iife_body.push b.letDeclaration(b.identifier('proto'), b.memberExpression(n.id, b.identifier('prototype')));
                        
                ctor = null
                methods.forEach  (m, mkey) =>
                        # if it's a method with name 'constructor' output the special ctor function
                        if mkey is 'constructor'
                                ctor = m
                        else
                                class_init_iife_body.push @create_proto_method m, n

                smethods.forEach (sm) =>
                        class_init_iife_body.push @create_static_method sm, n

                proto_props = @create_properties properties, n, false
                class_init_iife_body = class_init_iife_body.concat(proto_props) if proto_props?
                        
                static_props = @create_properties sproperties, n, true
                class_init_iife_body = class_init_iife_body.concat(static_props) if static_props?

                # generate and prepend a default ctor if there isn't one declared.
                # It looks like this in code:
                #   function Subclass (...args) { %super.call(this, args...); }
                if not ctor?
                        ctor = @create_default_constructor n
                        @method_stack.push ctor
                        class_element.value = @visit ctor.value
                        @method_stack.pop()

                        
                ctor_func = @create_constructor ctor, n
                if n.superClass?
                        class_init_iife_body.unshift(b.expressionStatement(b.assignmentExpression(b.memberExpression(b.memberExpression(n.id, b.identifier("prototype")), b.identifier("constructor")), "=", n.id)))
                        # also set ctor.prototype = Object.create(superClass.prototype)
                        l = b.memberExpression(ctor_func.id,b.identifier("prototype"))
                        r = b.callExpression(objectCreate_id, [b.memberExpression(superid, b.identifier("prototype"))])
                        class_init_iife_body.unshift b.expressionStatement(b.assignmentExpression(l, "=", r))

                        # 14.5.17 step 9, make sure the constructor's __proto__ is set to superClass
                        class_init_iife_body.unshift b.expressionStatement(b.callExpression(setPrototypeOf_id, [ctor_func.id, superid]))
                class_init_iife_body.unshift ctor_func


                # make sure we return the function from our iife
                class_init_iife_body.push b.returnStatement(n.id)

                #  (function (%super?) { ... })
                b.functionExpression(null, (if n.superClass? then [superid] else []), b.blockStatement(class_init_iife_body))

        gather_members: (ast_class) ->
                methods     = new Map
                smethods    = new Map
                properties  = new Map
                sproperties = new Map

                for class_element in ast_class.body.body
                        if class_element.static and class_element.key.name is "prototype"
                                reportError(SyntaxError, "Illegal method name 'prototype' on static class member.", @filename, class_element.loc)
                                
                        if class_element.kind is ""
                                # a method
                                method_map = if class_element.static then smethods else methods
                                if method_map.has(class_element.key.name)
                                        reportError(SyntaxError, "method '#{class_element.key.name}' has already been defined.", @filename, class_element.loc)
                                method_map.set(class_element.key.name, class_element)
                        else
                                # a property
                                property_map = if class_element.static then sproperties else properties

                                # XXX this is broken for computed properties
                                property_map.set(class_element.key, new Map) if not property_map.has(class_element.key)

                                if property_map.get(class_element.key).has(class_element.kind)
                                        reportError(SyntaxError, "a '#{class_element.kind}' method for '#{escodegen.generate class_element.key}' has already been defined.", @filename, class_element.loc)

                                property_map.get(class_element.key).set(class_element.kind, class_element)

                [properties, methods, sproperties, smethods]
                        
                
        create_constructor: (ast_method, ast_class) ->
                if ast_method.value.defaults?.type?
                        console.log escodegen.generate ast_method.value.defaults
                b.functionDeclaration(ast_class.id, ast_method.value.params, ast_method.value.body, ast_method.value.defaults, ast_method.value.rest, ast_method.value.loc)

        create_default_constructor: (ast_class) ->
                # splat args into the call to super's ctor if there's a superclass
                args_id = b.identifier('args');
                functionBody = b.blockStatement(if ast_class.superClass then [b.expressionStatement(b.callExpression(b.identifier('super'), [b.spreadElement(args_id)]))] else []);
                b.methodDefinition(b.identifier('constructor'), b.functionExpression(null, [], functionBody, [], args_id));
                
        create_proto_method: (ast_method, ast_class) ->
                proto_member = b.memberExpression(b.identifier('proto'), (if ast_method.key.type is ComputedPropertyKey then ast_method.key.expression else ast_method.key), ast_method.key.type is ComputedPropertyKey)
                
                method = b.functionExpression(b.identifier("#{ast_class.id.name}:#{ast_method.key.name}"), ast_method.value.params, ast_method.value.body, ast_method.value.defaults, ast_method.value.rest, ast_method.value.loc)

                b.expressionStatement(b.assignmentExpression(proto_member, "=", method))

        create_static_method: (ast_method, ast_class) ->
                method = b.functionExpression(ast_method.key, ast_method.value.params, ast_method.value.body, ast_method.value.defaults, ast_method.value.rest, ast_method.value.loc)

                b.expressionStatement(b.assignmentExpression(b.memberExpression(ast_class.id, (if ast_method.key.type is ComputedPropertyKey then ast_method.key.expression else ast_method.key), ast_method.key.type is ComputedPropertyKey), "=", method))

        create_properties: (properties, ast_class, are_static) ->
                propdescs = []

                properties.forEach (prop_map, prop) =>
                        accessors = []
                        key = null

                        getter = prop_map.get("get")
                        setter = prop_map.get("set")

                        if getter?
                                accessors.push b.property(b.identifier("get"), getter.value)
                                key = prop
                        if setter?
                                accessors.push b.property(b.identifier("set"), setter.value)
                                key = prop

                        propdescs.push b.property(key, b.objectExpression(accessors))

                return null if propdescs.length is 0

                propdescs_literal = b.objectExpression(propdescs)

                if are_static
                        target = ast_class.id
                else
                        target = b.identifier('proto')

                b.expressionStatement(b.callExpression(b.memberExpression(b.identifier("Object"), b.identifier("defineProperties")), [target, propdescs_literal]))

# this should move to echo-desugar.coffee

class HoistFuncDecls extends TransformPass
        visitFunction: (n) ->
                decls = new Map()
                n.body = @visit n.body, decls
                decls.forEach (fd) =>
                        n.body.body.unshift fd
                n
        
        visitBlock: (n, decls) ->
                return n if n.body.length == 0

                i = 0
                e = n.body.length
                while i < e
                        child = n.body[i]
                        if child.type is FunctionDeclaration
                                decls.set(child.id.name, @visit(child))
                                n.body.splice i, 1
                                e = n.body.length
                        else
                                i += 1
                n = super(n, decls)
                n

# convert all function declarations to variable assignments
# with named function expressions.
# 
# i.e. from:
#   function foo() { }
# to:
#   var foo = function foo() { }
# 
class FuncDeclsToVars extends TransformPass
        visitFunctionDeclaration: (n) ->
                if n.toplevel
                        n.body = @visit n.body
                        return n
                else
                        func_exp = n
                        func_exp.type = FunctionExpression
                        func_exp.body = @visit func_exp.body
                        return b.varDeclaration(b.identifier(n.id.name), func_exp)


class RemapIdentifiers extends TransformPass
        constructor: (options, @filename, @initial_mapping) ->
                @mappings = new Stack(@initial_mapping)
                super

        #visit: (n) ->
        #        return n if @currentMapping().isEmpty()
        #        super

        visitBlock: (n) ->
                # clone the mapping and push it onto the stack
                @mappings.push shallow_copy_object @currentMapping()
                super
                @mappings.pop()
                n

        visitVariableDeclarator: (n) ->
                # if the variable's name exists in the mapping clear it out
                @currentMapping()[n.id.name] = null

        visitObjectPattern: (n) ->
                @currentMapping()[prop.key] = null for prop in n.properties
                super

        visitCatchClause: (n) ->
                @mappings.push shallow_copy_object @currentMapping()
                @currentMapping()[n.param.name] = null
                super
                @mappings.pop()
                n
                
        visitFunction: (n) ->
                @currentMapping()[n.id.name] = null if n.id?

                @mappings.push shallow_copy_object @currentMapping()
                @currentMapping()[n.rest.name] = null if n.rest?
                super
                @mappings.pop()
                n
                
        visitIdentifier: (n) ->
                if hasOwnProperty.call @currentMapping(), n.name
                        mapped = @currentMapping()[n.name]
                        return mapped if mapped?
                n
                
        currentMapping: -> if @mappings.depth > 0 then @mappings.top else Object.create null
                
class DesugarLetLoopVars extends TransformPass
        vargen = startGenerator()
        freshLoopVar = (ident) -> "%loop_#{ident}_#{vargen()}"
        
        constructor: (options, @filename) ->
                super

        visitFor: (n) ->
                return n if n.init?.type isnt VariableDeclaration
                return n if n.init?.kind isnt 'let'

                # we have a loop that looks like:
                #
                #  for (let x = ...; $test; $update) {
                #      /* body, containing a function that closes over x */
                #  }
                #
                # we desugar this to:
                #
                #  for (var %loop_x = ...; $test(with x replaced with %loop_x); $update(with x replaced with %loop_x) {
                #    let x = %loop_x;
                #    try {
                #      /* body */
                #    }
                #    finally {
                #      %loop_x = x;
                #    }
                #  }

                n.init.kind = "var"
                
                mappings = Object.create null

                for decl in n.init.declarations
                        loopvar = b.identifier(freshLoopVar(decl.id.name))
                        mappings[decl.id.name] = loopvar
                        decl.id = loopvar
                        
                assignments = []
                new_body = b.blockStatement()
                
                for loopvar of mappings
                        # this gives us the "let x = %loop_x"
                        # assignments, so get our fresh binding per
                        # loop iteration
                        new_body.body.push b.variableDeclaration('let', b.identifier(loopvar), mappings[loopvar])
                        
                        # and this gives us the assignment we put in
                        # the finally block to capture changes made to
                        # the loop variable in the body
                        assignments.push b.expressionStatement(b.assignmentExpression(mappings[loopvar], '=', b.identifier(loopvar)))

                new_body.body.push b.tryStatement(n.body, [], b.blockStatement(assignments))

                remap = new RemapIdentifiers(@options, @filename, mappings)
                n.test = remap.visit(n.test)
                n.update = remap.visit(n.update)

                n.body = @visit new_body

                n
                
        
# hoists all vars to the start of the enclosing function, replacing
# any initializer with an assignment expression.  We also take the
# opportunity to convert the vars to lets at this point so by the time
# the LLVMIRVisitory gets to the tree there are only consts and lets
#
# i.e. from
#    {
#       ....
#       var x = 5;
#       ....
#    }
# to
#    {
#       let x;
#       ....
#       x = 5;
#       ....
#    }
#
# we also warn if x was already hoisted (if the decl for it already exists in the toplevel scope)
class HoistVars extends TransformPass
        constructor: (options, @filename) ->
                super
                @scope_stack = new Stack

        create_empty_declarator = (decl_name) -> b.variableDeclarator(b.identifier(decl_name), b.undefined())
                
        visitProgram: (n) ->
                vars = new Set
                @scope_stack.push { func: n, vars: vars }
                n = super
                @scope_stack.pop()

                return n if vars.size() is 0

                n.body.unshift b.letDeclaration(create_empty_declarator varname for varname in vars.keys())
                n
        
        visitFunction: (n) ->
                vars = new Set
                @scope_stack.push { func: n, vars: vars }
                n = super
                @scope_stack.pop()

                return n if vars.size() is 0

                n.body.body.unshift b.letDeclaration(create_empty_declarator varname for varname in vars.keys())
                n

        visitFor: (n) ->
                @skipExpressionStatement = true
                n.init = @visit n.init
                @skipExpressionStatement = false
                n.test = @visit n.test
                n.update = @visit n.update
                n.body = @visit n.body
                n
                        
        visitForIn: (n) ->
                if n.left.type is VariableDeclaration
                        @scope_stack.top.vars.add n.left.declarations[0].id.name
                        n.left = b.identifier n.left.declarations[0].id.name
                n.right = @visit n.right
                n.body = @visit n.body
                n

        visitForOf: (n) ->
                if n.left.type is VariableDeclaration
                        @scope_stack.top.vars.add n.left.declarations[0].id.name
                        n.left = b.identifier n.left.declarations[0].id.name
                n.right = @visit n.right
                n.body = @visit n.body
                n
                                                
        visitVariableDeclaration: (n) ->
                if n.kind is "var"
                        # check to see if there are any initializers, which we'll convert to assignment expressions
                        assignments = []
                        for i in [0...n.declarations.length]
                                if n.declarations[i].init?
                                        assignments.push b.assignmentExpression(b.identifier(n.declarations[i].id.name), "=", @visit n.declarations[i].init)

                        # vars are hoisted to the containing function's toplevel scope
                        for decl in n.declarations
                                if @scope_stack.top.vars.has decl.id.name
                                        reportWarning("multiple var declarations for `#{decl.id.name}' in this function.", @filename, n.loc)
                                @scope_stack.top.vars.add decl.id.name

                        return b.emptyStatement() if assignments.length is 0
                                
                        # now return the new assignments, which will replace the original variable
                        # declaration node.
                        if assignments.length > 1
                                assign_exp = b.sequenceExpression(assignments)
                        else
                                assign_exp = assignments[0]


                        if @skipExpressionStatement
                                return assign_exp
                        else
                                return b.expressionStatement(assign_exp)
                else
                        n = super
                n

# this pass converts all arrow functions to normal anonymous function
# expressions with a closed-over 'this'
#
# take the following:
#
# function foo() {
#   let mapper = (arr) => {
#     arr.map (el => el * this.x);
#   };
# }
#
# This will be compiled to:
#
# function foo() {
#   let _this_010 = this;
#   let mapper = function (arr) {
#     arr.map (function (el) { return el * _this_010.x; });
#   };
# }
#
# and the usual closure conversion stuff will make sure the bindings
# exists in the closure env as usual.
# 
DesugarArrowFunctions = class DesugarArrowFunctions extends TransformPass
        definesThis: (n) -> n.type is FunctionDeclaration or n.type is FunctionExpression
        
        constructor: ->
                super
                @mapping = []
                @thisGen = startGenerator()

        visitArrowFunctionExpression: (n) ->
                if n.expression
                        n.body = b.blockStatement([b.returnStatement(n.body)])
                        n.expression = false
                n = @visitFunction n
                n.type = FunctionExpression
                n

        visitThisExpression: (n) ->
                return b.undefined() if @mapping.length == 0

                topfunc = @mapping[0].func

                for m in @mapping
                        if @definesThis m.func
                                # if we're already on top, just return the existing thisExpression
                                return n if topfunc is m

                                return b.identifier m.id if m.id?

                                m.id = "_this_#{@thisGen()}"

                                m.prepend = b.letDeclaration(b.identifier(m.id), b.thisExpression())
                                
                                return b.identifier m.id

                reportError(SyntaxError, "no binding for 'this' available for arrow function", @filename, n.loc)
        
        visitFunction: (n) ->
                @mapping.unshift { func: n, id: null }
                n = super n
                m = @mapping.shift()
                if m.prepend?
                        n.body.body.unshift m.prepend
                n
                
                
#
#   This pass walks the tree and moves all function expressions to the toplevel.
#
#   at the point where this pass runs there are a couple of assumptions:
# 
#   1. there are no function declarations anywhere in the program.  They have all
#      been converted to 'var X = %makeClosure(%env_Y, function (%env) { ... })'
# 
#   2. There are no free variables in the function expressions.
#
class LambdaLift extends TransformPass
        constructor: (options, @filename) ->
                super
                @functions = []

        visitProgram: (program) ->
                super
                program.body = @functions.concat program.body
                program

        maybePrependScratchArea: (n) ->
                if n.scratch_size > 0
                        n.body.body.unshift b.expressionStatement(intrinsic createArgScratchArea_id, [ b.literal(n.scratch_size) ])
                
        visitFunctionDeclaration: (n) ->
                n.body = @visit n.body
                @maybePrependScratchArea n
                n

        visitFunctionExpression: (n) ->
                if n.displayName?
                        global_name = genGlobalFunctionName n.displayName, @filename
                else if n.id?.name?
                        global_name = genGlobalFunctionName n.id.name, @filename
                else
                        global_name = genAnonymousFunctionName(@filename)
                
                n.type = FunctionDeclaration
                n.id = b.identifier(global_name)

                @functions.push n

                n.body = @visit n.body

                @maybePrependScratchArea n

                return b.identifier(global_name)

        ###
        # XXX don't leave me in
        #
        # convert all properties to init, since escodegen can't seem to deal with get/set properties
        visitProperty: (n) ->
                n = super
                n.kind = "init"
                n
        ###


class NameAnonymousFunctions extends TransformPass
        visitAssignmentExpression: (n) ->
                n = super n
                lhs = n.left
                rhs = n.right

                # if we have the form
                #   <identifier> = function () { }
                # convert to:
                #   <identifier> = function <identifier> () { }
                #if lhs.type is Identifier and rhs.type is FunctionExpression and not rhs.id?.name
                #        rhs.display = <something pretty about the lhs>
                #
                if rhs.type is FunctionExpression and not rhs.id?.name
                        rhs.displayName = escodegen.generate lhs
                n

#
# special pass to inline some common idioms dealing with IIFEs
# (immediately invoked function expressions).
#
#    (function(x1, x2, ...) { ... body ... }(y1, y2, ...);
#
# This is a common way to provide scoping in ES5 and earlier.  It is
# unnecessary with the addition of 'let' in ES6.  We assume that all
# bindings in 'body' have been replace by 'let' or 'const' (meaning
# that all hoisting has been done.)
#
# we translate this form into the following equivalent inlined form:
#
#    {
#        let x1 = y1;
#        let x2 = y2;
#
#        ...
# 
#        { ... body ... }
#    }
#
# we limit the inlining to those where count(y) <= count(x).
# otherwise we'd need to ensure that the evaluation of the extra y's
# takes place before the body is executed, even if they aren't used.
#
# Another form we can optimize is the following:
#
#    (function() { body }).call(this)
#
# this form can be inlined directly as:
#
#    { body }
# 
class IIFEIdioms extends TreeVisitor

        constructor: ->
                super
                @function_stack = new Stack
                @iife_generator = startGenerator()

        visitFunction: (n) ->
                @function_stack.push n
                rv = super
                @function_stack.pop
                rv

        maybeInlineIIFE: (candidate, n) ->
                arity = candidate.arguments[0].arguments[1].params.length
                arg_count = candidate.arguments.length - 1 # %invokeClosure's first arg is the callee

                return n if arg_count > arity

                # at this point we know we have an IIFE in an expression statement, ala:
                #
                # (function(x, ...) { ...body...})(y, ...);
                #
                # so just inline { ...body... } in place of the
                # expression statement, after doing some magic to fix
                # up argument bindings (done here) and return
                # statements in the body (done in LLVMIRVisitor).
                # 
                iife_rv_id = b.identifier "%iife_rv_#{@iife_generator()}"

                replacement = b.blockStatement()

                replacement.body.push b.letDeclaration(iife_rv_id, b.undefined())

                for i in [0...arity]
                        replacement.body.push b.letDeclaration(candidate.arguments[0].arguments[1].params[i], if i <= arg_count then candidate.arguments[i+1] else b.undefined())
                        
                @function_stack.top.scratch_size = Math.max @function_stack.top.scratch_size, candidate.arguments[0].arguments[1].scratch_size

                body = candidate.arguments[0].arguments[1].body
                body.ejs_iife_rv = iife_rv_id
                body.fromIIFE = true

                replacement.body.push body

                if is_intrinsic(n.expression, "%setSlot")
                        n.expression.arguments[2] = intrinsic getLocal_id, [iife_rv_id]
                        replacement.body.push n
                else if is_intrinsic(n.expression, "%setGlobal") or is_intrinsic(n.expression, "%setLocal")
                        n.expression.arguments[1] = intrinsic getLocal_id, [iife_rv_id]
                        replacement.body.push n
                        
                return replacement

        maybeInlineIIFECall: (candidate, n) ->
                return n if candidate.arguments.length isnt 2 or candidate.arguments[1].type isnt ThisExpression

                iife_rv_id = b.identifier "%iife_rv_#{@iife_generator()}"

                replacement = b.blockStatement()

                replacement.body.push b.letDeclaration(iife_rv_id, b.undefined())

                body = candidate.arguments[0].object.arguments[1].body
                body.ejs_iife_rv = iife_rv_id
                body.fromIIFE = true

                replacement.body.push body

                if is_intrinsic(n.expression, "%setSlot")
                        n.expression.arguments[2] = intrinsic getLocal_id, [iife_rv_id]
                        replacement.body.push n
                else if is_intrinsic(n.expression, "%setGlobal") or is_intrinsic(n.expression, "%setLocal")
                        n.expression.arguments[1] = intrinsic getLocal_id, [iife_rv_id]
                        replacement.body.push n

                return replacement
        
        visitExpressionStatement: (n) ->
                isMakeClosure = (a) -> is_intrinsic(a, "%makeClosure") or is_intrinsic(a, "%makeAnonClosure") or is_intrinsic(a, "%makeClosureNoEnv")
                # bail out early if we know we aren't in the right place
                if is_intrinsic(n.expression, "%invokeClosure")
                        candidate = n.expression
                else if is_intrinsic(n.expression, "%setSlot")
                        candidate = n.expression.arguments[2]
                else if is_intrinsic(n.expression, "%setGlobal") or is_intrinsic(n.expression, "%setLocal")
                        candidate = n.expression.arguments[1]
                else
                        return n

                # at this point candidate should only be an invokeClosure intrinsic
                return n if not is_intrinsic(candidate, "%invokeClosure")

                if isMakeClosure candidate.arguments[0]
                        return @maybeInlineIIFE candidate, n
                else if candidate.arguments[0].type is MemberExpression and (isMakeClosure candidate.arguments[0].object) and candidate.arguments[0].property.name is "call"
                        return @maybeInlineIIFECall candidate, n
                else
                        return n
                        
class DesugarDestructuring extends TransformPass
        gen = startGenerator()
        fresh = () -> b.identifier "%destruct_tmp#{gen()}"

        # given an assignment { pattern } = id
        # 
        createObjectPatternBindings = (id, pattern, decls) ->
                for prop in pattern.properties
                        memberexp = b.memberExpression(id, prop.key)
                        
                        decls.push b.variableDeclarator(prop.key, memberexp)

                        if prop.value.type is Identifier
                                # this happens with things like:  function foo ({w:i}) { }
                                throw new Error "not sure how to handle this case.." if prop.value.name isnt prop.key.name
                        else if prop.value.type is ObjectPattern
                                createObjectPatternBindings(memberexp, prop.value, decls)
                        else if prop.value.type is ArrayPattern
                                createArrayPatternBindings(memberexp, prop.value, decls)
                        else
                                throw new Error "createObjectPatternBindings: prop.value.type = #{prop.value.type}"

        createArrayPatternBindings = (id, pattern, decls) ->
                el_num = 0
                seen_spread = false
                
                for el in pattern.elements
                        memberexp = b.memberExpression(id, b.literal(el_num), true)

                        if seen_spread
                                reportError(SyntaxError, "elements after spread element in array pattern", el.loc)
                                
                        if el.type is Identifier
                                decls.push b.variableDeclarator(el, memberexp)
                        else if el.type is ObjectPattern
                                p_id = fresh()

                                decls.push b.variableDeclarator(p_id, memberexp)

                                createObjectPatternBindings(p_id, el, decls)
                        else if el.type is ArrayPattern
                                p_id = fresh()

                                decls.push b.variableDeclarator(p_id, memberexp)

                                createArrayPatternBindings(p_id, el, decls)
                        else if el.type is SpreadElement
                                decls.push b.variableDeclarator(el.argument, b.callExpression(b.memberExpression(id, b.identifier("slice")), [b.literal(el_num)]))
                                seen_spread = true
                        else
                                throw new Error("createArrayPatternBindings #{el.type}")

                        el_num += 1

        visitFunction: (n) ->
                # we visit the formal parameters directly, rewriting
                # them as tmp arg names and adding 'let' decls for the
                # pattern identifiers at the top of the function's
                # body.
                new_params = []
                new_decls = []
                for p in n.params
                        if p.type is ObjectPattern
                                p_id = fresh()
                                new_params.push p_id
                                new_decl = b.letDeclaration()
                                createObjectPatternBindings(p_id, p, new_decl.declarations)
                                new_decls.push new_decl
                        else if p.type is ArrayPattern
                                p_id = fresh()
                                new_params.push p_id
                                new_decl = b.letDeclaration()
                                createArrayPatternBindings(p_id, p, new_decl.declarations)
                                new_decls.push new_decl
                        else if p.type is Identifier
                                # we just pass this along
                                new_params.push(p)
                        else
                                throw new Error "unhandled type of formal parameter in DesugarDestructuring #{p.type}"

                n.body.body = new_decls.concat(n.body.body)
                n.params = new_params
                n.body = @visit n.body
                n

        visitVariableDeclaration: (n) ->
                decls = []

                for decl in n.declarations
                        if decl.id.type is ObjectPattern
                                obj_tmp_id = fresh()
                                decls.push b.variableDeclarator(obj_tmp_id, @visit(decl.init))
                                createObjectPatternBindings(obj_tmp_id, decl.id, decls)
                        else if decl.id.type is ArrayPattern
                                # create a fresh tmp and declare it
                                array_tmp_id = fresh()
                                decls.push b.variableDeclarator(array_tmp_id, @visit(decl.init))
                                createArrayPatternBindings(array_tmp_id, decl.id, decls)
                                        
                        else if decl.id.type is Identifier
                                decl.init = @visit(decl.init)
                                decls.push(decl)
                        else
                                reportError(Error, "unhandled type of variable declaration in DesugarDestructuring #{decl.id.type}", @filename, n.loc)

                n.declarations = decls
                n

        visitAssignmentExpression: (n) ->
                if n.left.type is ObjectPattern or n.left.type is ArrayPattern
                        throw new Error "EJS doesn't support destructuring assignments yet (issue #16)"
                        reportError(Error, "cannot use destructuring with assignment operators other than '='", @filename, n.loc) if n.operator isnt "="
                else
                        super

class DesugarTemplates extends TransformPass
        # for template strings without a tag (i.e. of the form
        # `literal ${with}${possibly}${substitutions}`) we simply
        # inline the spec'ed behavior of the default handler (zipping
        # together the cooked values and substitutions to form the
        # result.)
        #
        # for tagged templates: (i.e. of the form tag`literal`) we
        # create a function which lazily generates the const/frozen
        # callsite_id, which is a unique object containing both raw
        # and cooked literal portions of the template literal.
        #
        # This function is invoked to get the callsiteId, which is
        # then passed (along with the array of substitutions) to the
        # handler named by "tag" above.
        # 
        callsiteGen = startGenerator()
        freshCallsiteId = () -> "%callsiteId_#{callsiteGen()}"

        visitBlock: (n, callsites) ->
                callsites = []
                n = super(n, callsites)
                # prepend the callsite generation functions (generated by desugaring tagged template expressions below)
                n.body = callsites.concat(n.body)
                n
                
        generateCreateCallsiteIdFunc: (name, quasis) ->
                raw_elements = []
                cooked_elements = []
                for q in quasis
                        raw_elements.push    b.literal q.value.raw
                        cooked_elements.push b.literal q.value.cooked

                qo = b.objectExpression([b.property(b.identifier("raw"), b.arrayExpression(raw_elements)),
                                         b.property(b.identifier("cooked"), b.arrayExpression(cooked_elements))])

                return b.functionDeclaration(b.identifier("generate_#{name}"), [],
                        b.blockStatement([b.expressionStatement(intrinsic templateCallsite_id, [b.literal(name), qo])]))
                        
        visitTaggedTemplateExpression: (n, callsites) ->
                callsiteid_func_id = freshCallsiteId();
                callsite_func = @generateCreateCallsiteIdFunc(callsiteid_func_id, n.quasi.quasis)
                callsites.push callsite_func
                callsiteid_func_call = b.callExpression(callsite_func.id, [])
                return b.callExpression(n.tag, [callsiteid_func_call].concat(n.quasi.expressions))
                
        visitTemplateLiteral: (n) ->
                cooked = b.arrayExpression((b.literal(q.value.cooked) for q in n.quasis))
                substitutions = b.arrayExpression(n.expressions)
                intrinsic templateDefaultHandlerCall_id, [cooked, substitutions]
                
                
# we split up assignment operators +=/-=/etc into their
# component operator + assignment so we can mark lhs as
# setLocal/setGlobal/etc, and rhs getLocal/getGlobal/etc
class DesugarUpdateAssignments extends TransformPass
        updateGen = startGenerator()
        freshUpdate = () -> "%update_#{updateGen()}"
        
        constructor: ->
                super
                @debug = true
                @updateGen = startGenerator()

        visitProgram: (n) ->
                n.prepends = []
                n = super(n, n)
                if n.prepends.length > 0
                        n.body = n.prepends.concat(n.body)
                n
                
        visitBlock: (n) ->
                n.prepends = []
                n = super(n, n)
                if n.prepends.length > 0
                        n.body = n.prepends.concat(n.body)
                n
        
        visitAssignmentExpression: (n, parentBlock) ->
                n = super(n, parentBlock)
                if n.operator.length is 2
                        if n.left.type is Identifier
                                # for identifiers we just expand a += b to a = a + b
                                n.right = b.binaryExpression(n.left, n.operator[0], n.right)
                                n.operator = '='
                        else if n.left.type is MemberExpression

                                complex_exp = (n) ->
                                        return false if not n?
                                        return false if n.type is Literal
                                        return false if n.type is Identifier
                                        return true

                                prepend_update = () ->
                                        update_id = b.identifier freshUpdate()
                                        parentBlock.prepends.unshift b.letDeclaration(update_id, b.null())
                                        update_id
                                
                                object_exp = n.left.object
                                prop_exp = n.left.property

                                expressions = []

                                if complex_exp object_exp
                                        update_id = prepend_update()
                                        expressions.push b.assignmentExpression(update_id, '=', object_exp)
                                        n.left.object = update_id

                                if complex_exp prop_exp
                                        update_id = prepend_update()
                                        expressions.push b.assignmentExpression(update_id, '=', prop_exp)
                                        n.left.property = update_id

                                n.right = b.binaryExpression(b.memberExpression(n.left.object, n.left.property, n.computed), n.operator[0], n.right)
                                n.operator = "="
                                
                                if expressions.length isnt 0
                                        expressions.push n
                                        return b.sequenceExpression(expressions)
                                n
                        else
                                reportError(Error, "unexpected expression type #{n.left.type} in update assign expression.", @filename, n.left.loc)
                n

class DesugarImportExport extends TransformPass
        freshId = do ->
                importGen = startGenerator()
                (prefix) -> b.identifier "%#{prefix}_#{importGen()}"

        constructor: (options, @filename, @allModules) ->
                super

        visitFunction: (n) ->
                return n if not n.toplevel
                
                @exports = []
                @batch_exports = []

                super
                
        visitImportDeclaration: (n) ->
                if n.specifiers.length is 0
                        # no specifiers, it's of the form:  import from "foo"
                        # don't waste a decl for this type
                        return b.expressionStatement(intrinsic(moduleGetExotic_id, [n.source_path]))

                import_decls =  b.letDeclaration()

                module = @allModules.get(n.source_path.value)
                for spec in n.specifiers
                        if spec.kind is "default"
                                #
                                # let #{n.specifiers[0].id} = %import_decl.default
                                #
                                if not module.hasDefaultExport()
                                        reportError(ReferenceError, "module `#{n.source_path.value}' doesn't have default export", @filename, n.loc)

                                import_decls.declarations.push b.variableDeclarator(n.specifiers[0].id, intrinsic(moduleGetSlot_id, [n.source_path, b.literal("default")]))

                        else if spec.kind is "named"
                                #
                                # let #{spec.id} = %import_decl.#{spec.name || spec.id }
                                #
                                if not module.exports.has(spec.id.name)
                                        reportError(ReferenceError, "module `#{n.source_path.value}' doesn't export `#{spec.id.name}'", @filename, spec.id.loc)
                                import_decls.declarations.push b.variableDeclarator(spec.name or spec.id, intrinsic(moduleGetSlot_id, [n.source_path, b.literal(spec.id.name)]))

                        else # if spec.kind is "batch"
                                # let #{spec.name} = %import_decl
                                import_decls.declarations.push b.variableDeclarator(spec.name, intrinsic(moduleGetExotic_id, [n.source_path]))
                        
                return import_decls

        visitExportDeclaration: (n) ->
                # handle the following case in two parts:
                #    export * from "foo"

                if n.source?
                        # we either have:
                        #   export * from "foo"
                        # or
                        #   export { ... } from "foo"

                        # import the module regardless
                        import_tmp = freshId("import")

                        export_stuff = [b.letDeclaration(import_tmp, intrinsic(moduleGetExotic_id, [n.source_path]))]                             
                        if n.default
                                # export * from "foo"
                                if n.specifiers.length isnt 1 or n.specifiers[0].type isnt ExportBatchSpecifier
                                        reportError(SyntaxError, "invalid export", @filename, n.loc)
                                @batch_exports.push { source: import_tmp, specifiers: [] }
                        else
                                # export { ... } from "foo"
                                for spec in n.specifiers
                                        if not @allModules.get(n.source_path.value).exports.has(spec.id.name)
                                                reportError(ReferenceError, "module `#{n.source_path.value}' doesn't export `#{spec.id.name}'", @filename, spec.id.loc)

                                        export_name = spec.name or spec.id

                                        export_stuff.push(b.expressionStatement(intrinsic(moduleSetSlot_id, [b.literal(@filename), b.literal(export_name.name), b.memberExpression(import_tmp, spec.id)])));
                        return export_stuff

                # export function foo () { ... }
                if n.declaration.type is FunctionDeclaration
                        @exports.push { id: n.declaration.id }

                        # we're going to pass it to the moduleSetSlot intrinsic, so it needs to be an expression (or else escodegen freaks out)
                        n.declaration.type = FunctionExpression
                        return b.expressionStatement(intrinsic(moduleSetSlot_id, [b.literal(@filename), b.literal(n.declaration.id.name), @visit(n.declaration)]))

                # export class Foo () { ... }
                if n.declaration.type is ClassDeclaration
                        @exports.push { id: n.declaration.id }

                        n.declaration.type = ClassExpression
                        return b.expressionStatement(intrinsic(moduleSetSlot_id, [b.literal(@filename), b.literal(n.declaration.id.name), @visit(n.declaration)]))

                # export let foo = bar;
                if n.declaration.type is VariableDeclaration
                        export_defines = []
                        for decl in n.declaration.declarations
                                @exports.push({ id: decl.id })
                                export_defines.push(b.expressionStatement(intrinsic(moduleSetSlot_id, [b.literal(@filename), b.literal(decl.id.name), @visit(decl.init)])))
                        return export_defines

                # export foo = bar;
                if n.declaration.type is VariableDeclarator
                        @exports.push { id: n.declaration.id }
                        return b.expressionStatement(intrinsic(moduleSetSlot_id, [b.literal(@filename), b.literal(n.declaration.id.name), @visit(n.declaration)]))

                # export default ...;
                # 
                if n.default
                        return b.expressionStatement(intrinsic(moduleSetSlot_id, [b.literal(@filename), b.literal("default"), @visit(n.declaration)]))

                reportError(SyntaxError, "Unsupported type of export declaration #{n.declaration.type}", @filename, n.loc)

        visitModuleDeclaration: (n) ->
                init = intrinsic(moduleGetExotic_id, [n.source_path])
                return b.letDeclaration(n.id, init)

#
# convert from:
#
#   function name (arg1, arg2, arg3, ...rest) {
#     // body
#   }
#
# to:
#
#   function name (arg1, arg2, arg3) {
#     let rest = %gatherRest('rest', 3);
#     // body
#   }
# 
class DesugarRestParameters extends TransformPass
        visitFunction: (n) ->
                n = super
                if n.rest?
                        rest_declaration = b.letDeclaration(n.rest, b.callExpression(gatherRest_id, [b.literal(n.rest.name), b.literal(n.params.length)]))
                        n.body.body.unshift rest_declaration
                        n.rest = undefined
                n

#
# desugars
#
#   [1, 2, ...foo, 3, 4]
# 
#   o.foo(1, 2, ...foo, 3, 4)
#
# to:
#
#   %arrayFromSpread([1, 2], foo, [3, 4])
#
#   foo.apply(o, %arrayFromSpread([1, 2], foo, [3, 4])
#
class DesugarSpread extends TransformPass
        visitArrayExpression: (n) ->
                n = super
                needs_desugaring = false
                for el in n.elements
                        if el.type is SpreadElement
                                needs_desugaring = true

                if needs_desugaring
                        new_args = []
                        current_elements = []
                        for el in n.elements
                                if el.type is SpreadElement
                                        if current_elements.length is 0
                                                # just push the spread argument into the new args
                                                new_args.push el.argument
                                        else
                                                # push the current_elements as an array literal, then the spread.
                                                # also reset current_elements to []
                                                new_args.push b.arrayExpression(current_elements)
                                                new_args.push el.argument
                                                current_elements = []
                                else
                                        current_elements.push el
                        if current_elements.length isnt 0
                                new_args.push b.arrayExpression(current_elements)

                        # check to see if we've just created an array of nothing but array literals, and flatten them all
                        # into one and get rid of the spread altogether
                        all_arrays = true
                        for a in new_args
                                all_arrays = false if a.type isnt ArrayExpression
                                
                        if all_arrays
                                na = []
                                for a in new_args
                                       na = na.concat a.elements
                                n.elements = na
                                return n
                        else
                                return intrinsic arrayFromSpread_id, new_args

                n

        visitCallExpression: (n) ->
                n = super
                needs_desugaring = false
                for el in n.arguments
                        if el.type is SpreadElement
                                needs_desugaring = true

                if needs_desugaring
                        new_args = []
                        current_elements = []
                        for el in n.arguments
                                if is_intrinsic(el, "%arrayFromSpread")
                                        # flatten spreads
                                        new_args.concat el.arguments
                                                
                                else if el.type is SpreadElement
                                        if current_elements.length is 0
                                                # just push the spread argument into the new args
                                                new_args.push el.argument
                                        else
                                                # push the current_elements as an array literal, then the spread.
                                                # also reset current_elements to []
                                                new_args.push b.arrayExpression(current_elements)
                                                new_args.push el.argument
                                                current_elements = []
                                else
                                        current_elements.push el

                        if current_elements.length isnt 0
                                new_args.push b.arrayExpression(current_elements)

                        # check to see if we've just created an array of nothing but array literals, and flatten them all
                        # into one and get rid of the spread altogether
                        all_arrays = true
                        for a in new_args
                                all_arrays = false if a.type isnt ArrayExpression
                        if all_arrays
                                na = []
                                for a in new_args
                                       na = na.concat a.elements

                                n.arguments = na
                        else
                                if n.callee.type is MemberExpression
                                        receiver = n.callee.object
                                else
                                        receiver = b.null()

                                n.callee = b.memberExpression(n.callee, b.identifier("apply"))
                                n.arguments = [receiver, intrinsic(arrayFromSpread_id, new_args)]
                n

#
# desugars
#
#   function (a, b = a) { ... }
#
# to:
#
#   function (a, b) {
#     a = %argPresent(1) ? %getArg(0) : undefined;
#     b = %argPresent(2) ? %getArg(1) : a;
#   }
#
class DesugarDefaults extends TransformPass
        constructor: (options, @filename) ->
                super

        visitFunction: (n) ->
                n = super(n)

                prepends = []
                seen_default = false

                n.params.forEach (p, i) =>
                        d = n.defaults[i]
                        if d?
                                seen_default = true
                        else
                                if seen_default
                                        reportError(SyntaxError, "Cannot specify non-default parameter after a default parameter", @filename, p.loc)
                                d = b.undefined()
                        prepends.push(b.letDeclaration(p, b.conditionalExpression(intrinsic(argPresent_id, [b.literal(i+1), b.literal(n.defaults[i]?)]), intrinsic(getArg_id, [b.literal(i)]), d)))
                n.body.body = prepends.concat(n.body.body)
                n.defaults = []
                n

class DesugarArguments extends TransformPass
        constructor: (options, @filename) ->
                super

        visitIdentifier: (n) ->
                return intrinsic(getArgumentsObject_id) if n.name is "arguments"
                super

        visitVariableDeclarator: (n) ->
                if n.id.name is "arguments"
                        reportError(SyntaxError, "Cannot declare variable named 'arguments'", @filename, n.id.loc)
                super

        visitAssignmentExpression: (n) ->
                if n.left.type is Identifier and n.left.name is "arguments"
                        reportError(SyntaxError, "Cannot set 'arguments'", @filename, n.left.loc)
                super

        visitProperty: (n) ->
                if n.key.type is ComputedPropertyKey
                        n.key = @visit n.key
                n.value = @visit n.value
                n
                
                
# desugars
#
#   for (let x of a) { ... }
#
# to:
#
#   {
#     %forof = a[Symbol.iterator]();
#     while (!(%iter_next = %forof.next()).done) {
#       let x = %iter_next.value;
#       { ... }
#     }
#   }
class DesugarForOf extends TransformPass
        forofgen = startGenerator()
        freshForOf = (ident='') -> "%forof#{ident}_#{forofgen()}"

        constructor: ->
                super
                @function_stack = new Stack

        visitFunction: (n) ->
                @function_stack.push n
                rv = super
                @function_stack.pop
                rv

        visitForOf: (n) ->
                n.left = @visit n.left
                n.right = @visit n.right
                n.body = @visit n.body
                
                iterable_tmp   = freshForOf('tmp')
                iter_name      = freshForOf('iter')
                iter_next_name = freshForOf('next')
                
                iterable_id  = b.identifier iterable_tmp
                iter_id      = b.identifier iter_name
                iter_next_id = b.identifier iter_next_name

                tmp_iterable_decl = b.letDeclaration(iterable_id, n.right)

                Symbol_iterator = b.memberExpression(b.identifier("Symbol"), b.identifier("iterator"))
                get_iterator_stmt = b.letDeclaration(iter_id, b.callExpression(b.memberExpression(iterable_id, Symbol_iterator, true), []))

                if n.left.type is VariableDeclaration
                        loop_iter_stmt = b.letDeclaration(n.left.declarations[0].id # can there be more than 1?
                                                          b.memberExpression(iter_next_id, b.identifier("value")))
                else
                        loop_iter_stmt = b.expressionStatement(b.assignmentExpression(n.left, "=", b.memberExpression(iter_next_id, b.identifier("value"))))

                next_decl = b.letDeclaration(iter_next_id, b.undefined())

                not_done = b.unaryExpression("!", b.memberExpression(b.assignmentExpression(iter_next_id, "=", b.callExpression(b.memberExpression(iter_id, b.identifier("next")))), b.identifier("done")))
                
                while_stmt = b.whileStatement(not_done, b.blockStatement([loop_iter_stmt, n.body]))

                return b.blockStatement([
                        tmp_iterable_decl
                        get_iterator_stmt
                        next_decl
                        while_stmt
                ])
                
                
# switch this to true if you want to experiment with the new CFA2
# code.  definitely, definitely not ready for prime time.
enable_cfa2 = false

# the HoistFuncDecls phase transforms the AST to give v8 semantics
# when faced with multiple function declarations within the same
# function scope.
#
enable_hoist_func_decls_pass = true

class NewClosureConvert
        constructor: (@options, @filename, @allModules) ->

        visit: (tree) ->
                new_cc.Convert(@options, @filename, tree, @allModules)
                
passes = [
        DesugarImportExport
        DesugarClasses
        DesugarDestructuring
        DesugarUpdateAssignments
        DesugarTemplates
        DesugarArrowFunctions
        DesugarDefaults
        DesugarRestParameters
        DesugarForOf
        DesugarSpread
        HoistFuncDecls if enable_hoist_func_decls_pass
        FuncDeclsToVars
        DesugarLetLoopVars
        HoistVars
        NameAnonymousFunctions
        DesugarArguments
        #CFA2 if enable_cfa2
        NewClosureConvert
        LambdaLift
        ]

exports.convert = (tree, filename, modules, options) ->
        debug.log "before:"
        debug.log -> escodegen.generate tree

        passes.forEach (passType) ->
                return if not passType?
                try
                        pass = new passType(options, filename, modules)
                        tree = pass.visit tree
                        if options.debug_passes.has(passType.name)
                                console.log "after: #{passType.name}"
                                console.log escodegen.generate tree

                        debug.log 2, "after: #{passType.name}"
                        debug.log 2, -> escodegen.generate tree
                        debug.log 3, -> __ejs.GC.dumpAllocationStats "after #{passType.name}" if __ejs?
                catch e
                        debug.log 2, "exception in pass #{passType.name}"
                        debug.log 2, e
                        throw e

        tree
