esprima = require 'esprima'
escodegen = require 'escodegen'
debug = require 'debug'

b = require 'ast-builder'

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
makeClosure_id          = b.identifier "%makeClosure"
makeAnonClosure_id      = b.identifier "%makeAnonClosure"
setSlot_id              = b.identifier "%setSlot"
slot_id                 = b.identifier "%slot"
invokeClosure_id        = b.identifier "%invokeClosure"
setLocal_id             = b.identifier "%setLocal"
setGlobal_id            = b.identifier "%setGlobal"
getLocal_id             = b.identifier "%getLocal"
getGlobal_id            = b.identifier "%getGlobal"
getArg_id               = b.identifier "%getArg"
moduleGet_id            = b.identifier "%moduleGet"
moduleImportBatch_id    = b.identifier "%moduleImportBatch"
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
                b.functionDeclaration(ast_class.id, ast_method.value.params, ast_method.value.body, ast_method.value.defaults, ast_method.value.rest)

        create_default_constructor: (ast_class) ->
                # splat args into the call to super's ctor if there's a superclass
                args_id = b.identifier('args');
                functionBody = b.blockStatement(if ast_class.superClass then [b.expressionStatement(b.callExpression(b.identifier('super'), [b.spreadElement(args_id)]))] else []);
                b.methodDefinition(b.identifier('constructor'), b.functionExpression(null, [], functionBody, [], args_id));
                
        create_proto_method: (ast_method, ast_class) ->
                proto_member = b.memberExpression(b.memberExpression(ast_class.id, b.identifier('prototype')), (if ast_method.key.type is ComputedPropertyKey then ast_method.key.expression else ast_method.key), ast_method.key.type is ComputedPropertyKey)
                
                method = b.functionExpression(ast_method.key, ast_method.value.params, ast_method.value.body, ast_method.value.defaults, ast_method.value.rest)

                b.expressionStatement(b.assignmentExpression(proto_member, "=", method))

        create_static_method: (ast_method, ast_class) ->
                method = b.functionExpression(ast_method.key, ast_method.value.params, ast_method.value.body, ast_method.value.defaults, ast_method.value.rest)

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
                        if prop.set?
                                accessors.push b.property(b.identifier("set"), setter.value)
                                key = prop

                        propdescs.push b.property(key, b.objectExpression(accessors))

                return null if propdescs.length is 0

                propdescs_literal = b.objectExpression(propdescs)

                if are_static
                        target = ast_class.id
                else
                        target = b.memberExpression(ast_class.id, b.identifier("prototype"))

                b.expressionStatement(b.callExpression(b.memberExpression(b.identifier("Object"), b.identifier("defineProperties")), [target, propdescs_literal]))

#
# each element in env is an object of the form:
#   { exp: ...     // the AST node corresponding to this environment.  will be a function or a BlockStatement. (right now just functions)
#     id:  ...     // the number/id of this environment
#     decls: ..    // a set of the variable names that are declared in this environment
#     closed: ...  // a set of the variable names that are used from this environment (i.e. the ones that need to move to the environment)
#     parent: ...  // the parent environment object
#   }
# 
LocateEnv = class LocateEnv extends TransformPass
        constructor: ->
                super
                @envs = new Stack
                @env_id = 0

        visitFunction: (n) ->
                current_env = @envs.top if @envs.depth > 0
                new_env = { id: @env_id, decls: (if n.ejs_decls? then n.ejs_decls else new Set), closed: new Set, parent: current_env }
                @env_id += 1
                n.ejs_env = new_env
                @envs.push new_env
                # don't visit the parameters for the same reason we don't visit the id's of variable declarators below:
                # we don't want a function to look like:  function foo (%env_1.x) { }
                # instead of                              function foo (x) { }
                n.body = @visit n.body
                @envs.pop()
                n

        visitVariableDeclarator: (n) ->
                # don't visit the id, as that will cause us to mark it ejs_substitute = true, and we'll end up with things like:
                #   var %env_1.x = blah
                # instead of what we want:
                #   var x = blah
                n.init = @visit n.init
                n

        visitProperty: (n) ->
                if n.key.type is ComputedPropertyKey
                        n.key = @visit n.key
                n.value = @visit n.value
                n

        visitIdentifier: (n) ->
                # find the environment in the env-stack that includes this variable's decl.  add it to the env's .closed set.
                current_env = @envs.top
                env = current_env

                # if the current environment declares that identifier, nothing to do.
                return n if env.decls.has n.name

                closed_over = false
                # look up our environment stack for the decl for it.
                env = env.parent
                while env?
                        if env.decls?.has n.name
                                closed_over = true
                                env = null
                        else
                                env = env.parent

                # if we found it higher on the function stack, we need to walk back up the stack forcing environments along the way, and make
                # sure the frame that declares it knows that something down the stack closes over it.
                if closed_over
                        env = current_env.parent
                        while env?
                                if env.decls?.has n.name
                                        env.closed.add n.name
                                        env = null
                                else
                                        env.nested_requires_env = true
                                        env = env.parent
                
                n.ejs_substitute = true
                n

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
                

# this class really doesn't behave like a normal TreeVisitor, as it modifies the tree in-place.
# XXX reformulate this as a TreeVisitor subclass.
class ComputeFree extends TransformPass
        constructor: ->
                super
                @call_free = @free.bind @
                @setUnion = Set.union
                
        visit: (n) ->
                @free n
                n

        decl_names: (arr) ->
                result = []
                for n in arr
                        if n.declarations?
                                result = result.concat (decl.id.name for decl in n.declarations)
                        else if n.id?
                                result.push n.id.name
                new Set result

        id_names: (arr) ->
                new Set arr.map ((id) -> id.name)

        collect_decls: (body) ->
                statement for statement in body when statement.type is VariableDeclaration or statement.type is FunctionDeclaration

        free_blocklike: (exp,body) ->
                if body?
                        decls = @decl_names @collect_decls body
                        uses = @setUnion.apply null, body.map @call_free
                else
                        decls = []
                        uses = new Set()
                exp.ejs_decls = decls
                exp.ejs_free_vars = uses.subtract decls
                exp.ejs_free_vars

        # TODO: move this into the @visit method
        free: (exp) ->
                return new Set if not exp?

                # define the properties we'll be filling in below, so that we can make them non-enumerable
                Object.defineProperty exp, "ejs_decls",     { value: undefined, writable: true, configurable: true }
                Object.defineProperty exp, "ejs_free_vars", { value: undefined, writable: true, configurable: true }
                
                switch exp.type
                        when Program
                                decls = @decl_names @collect_decls exp.body
                                uses = @setUnion.apply null, exp.body.map @call_free
                                exp.ejs_decls = decls
                                exp.ejs_free_vars = uses.subtract decls
                        when FunctionDeclaration
                                # this should only happen for the toplevel function we create to wrap the source file
                                param_names = @id_names exp.params
                                param_names.add exp.rest.name if exp.rest?
                                exp.ejs_free_vars = @free(exp.body).subtract param_names
                                exp.ejs_decls = exp.body.ejs_decls.union param_names
                        when FunctionExpression
                                param_names = @id_names exp.params
                                param_names.add exp.rest.name if exp.rest?
                                exp.ejs_free_vars = @free(exp.body).subtract param_names
                                exp.ejs_decls = param_names.union exp.body.ejs_decls
                        when ArrowFunctionExpression
                                param_names = @id_names exp.params
                                param_names.add exp.rest.name if exp.rest?
                                exp.ejs_free_vars = @free(exp.body).subtract param_names
                                exp.ejs_decls = param_names.union exp.body.ejs_decls
                        when LabeledStatement      then exp.ejs_free_vars = @free exp.body
                        when BlockStatement        then exp.ejs_free_vars = @free_blocklike exp, exp.body
                        when TryStatement          then exp.ejs_free_vars = @setUnion.apply null, [@free(exp.block)].concat exp.handlers.map @call_free
                        when CatchClause
                                param_set = if exp.param?.name? then new Set [exp.param.name] else new Set
                                exp.ejs_free_vars = @free(exp.body).subtract param_set
                                exp.ejs_decls = exp.body.ejs_decls.union param_set
                        when VariableDeclaration   then exp.ejs_free_vars = @setUnion.apply null, exp.declarations.map @call_free
                        when VariableDeclarator    then exp.ejs_free_vars = @free exp.init
                        when ExpressionStatement   then exp.ejs_free_vars = @free exp.expression
                        when Identifier            then exp.ejs_free_vars = new Set [exp.name]
                        when ThrowStatement        then exp.ejs_free_vars = @free exp.argument
                        when ForStatement          then exp.ejs_free_vars = @setUnion.call null, @free(exp.init), @free(exp.test), @free(exp.update), @free(exp.body)
                        when ForInStatement        then exp.ejs_free_vars = @setUnion.call null, @free(exp.left), @free(exp.right), @free(exp.body)
                        when ForOfStatement        then exp.ejs_free_vars = @setUnion.call null, @free(exp.left), @free(exp.right), @free(exp.body)
                        when WhileStatement        then exp.ejs_free_vars = @setUnion.call null, @free(exp.test), @free(exp.body)
                        when DoWhileStatement      then exp.ejs_free_vars = @setUnion.call null, @free(exp.test), @free(exp.body)
                        when SwitchStatement       then exp.ejs_free_vars = @setUnion.apply null, [@free exp.discriminant].concat exp.cases.map @call_free
                        when SwitchCase            then exp.ejs_free_vars = @free_blocklike exp, exp.consequent
                        when EmptyStatement        then exp.ejs_free_vars = new Set
                        when BreakStatement        then exp.ejs_free_vars = new Set
                        when ContinueStatement     then exp.ejs_free_vars = new Set
                        when SpreadElement         then exp.ejs_free_vars = @free exp.argument
                        when UpdateExpression      then exp.ejs_free_vars = @free exp.argument
                        when ReturnStatement       then exp.ejs_free_vars = @free exp.argument
                        when UnaryExpression       then exp.ejs_free_vars = @free exp.argument
                        when BinaryExpression      then exp.ejs_free_vars = @free(exp.left).union @free exp.right
                        when LogicalExpression     then exp.ejs_free_vars = @free(exp.left).union @free exp.right
                        when MemberExpression      then exp.ejs_free_vars = @free(exp.object) # we don't traverse into the property
                        when CallExpression        then exp.ejs_free_vars = @setUnion.apply null, [@free exp.callee].concat exp.arguments.map @call_free
                        when NewExpression         then exp.ejs_free_vars = @setUnion.apply null, [@free exp.callee].concat exp.arguments.map @call_free
                        when SequenceExpression    then exp.ejs_free_vars = @setUnion.apply null, exp.expressions.map @call_free
                        when ConditionalExpression then exp.ejs_free_vars = @setUnion.call null, @free(exp.test), @free(exp.consequent), @free(exp.alternate)
                        when TaggedTemplateExpression then exp.ejs_free_vars = @free exp.quasi
                        when TemplateLiteral       then exp.ejs_free_vars = @setUnion.apply null, exp.expressions.map @call_free
                        when Literal               then exp.ejs_free_vars = new Set
                        when ThisExpression        then exp.ejs_free_vars = new Set
                        when ComputedPropertyKey   then exp.ejs_free_vars = @free exp.expression
                        when Property
                                exp.ejs_free_vars = @free exp.value
                                if exp.key.type is ComputedPropertyKey
                                        # we only do this when the key is computed, or else identifiers show up as free
                                        exp.ejs_free_vars = exp.ejs_free_vars.union @free exp.key
                                exp.ejs_free_vars
                        when ObjectExpression
                                exp.ejs_free_vars = if exp.properties.length is 0 then (new Set) else @setUnion.apply null, (@free p for p in exp.properties)
                        when ArrayExpression
                                exp.ejs_free_vars = if exp.elements.length is 0 then (new Set) else @setUnion.apply null, exp.elements.map @call_free
                        when IfStatement           then exp.ejs_free_vars = @setUnion.call null, @free(exp.test), @free(exp.consequent), @free(exp.alternate)
                        when AssignmentExpression  then exp.ejs_free_vars = @free(exp.left).union @free exp.right
                        when ModuleDeclaration     then exp.ejs_free_vars = @free exp.body
                        when ExportDeclaration
                                exp.ejs_free_vars = @free exp.declaration
                                exp.ejs_decls = exp.declaration.ejs_decls
                        when ImportDeclaration
                                exp.ejs_decls = new Set exp.specifiers.map ((specifier) -> specifier.id.name)
                                # no free vars in an ImportDeclaration
                                exp.ejs_free_vars = new Set
                                
                        else throw "Internal error: unhandled node '#{JSON.stringify exp}' in free()"
                exp.ejs_free_vars

                
                
# 1) allocates the environment at the start of the n
# 2) adds mappings for all .closed variables
class SubstituteVariables extends TransformPass
        constructor: ->
                super
                @function_stack = new Stack
                @mappings = new Stack

        currentMapping: -> if @mappings.depth > 0 then @mappings.top else Object.create null

        visitIdentifier: (n) ->
                if hasOwnProperty.call @currentMapping(), n.name
                        return @currentMapping()[n.name]
                n

        visitFor: (n) ->
                # for loops complicate things.
                # if any of the variables declared in n.init are closed over
                # we promote all of them outside of n.init.

                @skipExpressionStatement = true
                init = @visit n.init
                @skipExpressionStatement = false
                n.test = @visit n.test
                n.update = @visit n.update
                n.body = @visit n.body
                if Array.isArray init
                        n.init = null
                        return b.blockStatement(init.concat([n]))

                n.init = init
                n

        visitForIn: (n) ->
                # for-in loops complicate things.

                left = @visit n.left
                n.right = @visit n.right
                n.body = @visit n.body
                if Array.isArray left
                        console.log "whu?"
                        n.left = b.identifier(left[0].declarations[0].id.name)
                        return b.blockStatement(left.concat([n]))

                n.left = left
                n

        visitVariableDeclaration: (n) ->
                # here we do some magic depending on whether or not
                # variables are closed over (i.e. pushed into the
                # environment).
                # 
                # 1. for variables that are closed over that aren't
                #    initialized (that is, they're implicitly
                #    'undefined'), we just remove their declaration
                #    entirely.  it's already been converted to a slot
                #    everywhere else, and env slots are explicitly
                #    initialized to undefined by the runtime.
                #
                # 2. for variables that are closed over that *are*
                #    initialized, we splice them into the list and
                #    split the VariableDeclaration node into two, so
                #    if 'y' is closed over in the following input:
                #
                #    let x = 2, y = x * 2, z = 10;
                #
                #    we'll end up with this in the output:
                #
                #    let x = 2;
                #    %slot(%env, 1, 'y') = x * 2;
                #    let z = 10;
                #
                decls = n.declarations

                rv = []
                
                new_declarations = []

                # we loop until we find a variable that's closed over *and* has an initializer.
                for decl in decls
                        decl.init = @visit decl.init

                        closed_over = hasOwnProperty.call @currentMapping(), decl.id.name
                        if closed_over
                                # for variables that are closed over but undefined, we skip them (thereby removing them from the list of decls)

                                if decl.init? # FIXME: we should also check for an explicit 'undefined' here

                                        # push the current set of new_declarations if there are any
                                        if new_declarations.length > 0
                                                rv.push b.variableDeclaration(n.kind, new_declarations)

                                        # splice in this assignment
                                        rv.push b.expressionStatement(b.assignmentExpression(@currentMapping()[decl.id.name], "=", decl.init))

                                        # then re-init the new_declarations array
                                        new_declarations = []
                        
                        else
                                # for variables that aren't closed over, we just add them to the currect decl list.
                                new_declarations.push decl
                        

                # push the last set of new_declarations if there were any
                if new_declarations.length > 0
                        rv.push b.variableDeclaration(n.kind, new_declarations)

                if rv.length is 0
                        rv = b.emptyStatement()
                rv

        visitProperty: (n) ->
                if n.key.type is ComputedPropertyKey
                        n.key = @visit n.key
                n.value = @visit n.value
                n

        visitFunctionBody: (n) ->
                n.scratch_size = 0
                
                # we use this instead of calling super in visitFunction because we don't want to visit parameters
                # during this pass, or they'll be substituted with an %env.

                @function_stack.push n
                n.body = @visit n.body
                @function_stack.pop()
                
                n

        visitFunction: (n) ->
            try
                this_env_id = b.identifier("%env_#{n.ejs_env.id}")
                if n.ejs_env.parent?
                        parent_env_name = "%env_#{n.ejs_env.parent.id}"

                env_prepends = []
                new_mapping = shallow_copy_object @currentMapping()
                if n.ejs_env.closed.empty() and not n.ejs_env.nested_requires_env
                        env_prepends.push b.letDeclaration(this_env_id, b.null())
                else
                        # insert environment creation (at the start of the function body)
                        env_prepends.push b.letDeclaration(this_env_id, intrinsic makeClosureEnv_id, [b.literal n.ejs_env.closed.size() + if n.ejs_env.parent? then 1 else 0])

                        n.ejs_env.slot_mapping = Object.create null
                        i = 0
                        if n.ejs_env.parent?
                                n.ejs_env.slot_mapping[parent_env_name] = i
                                i += 1
                        n.ejs_env.closed.map (el) ->
                                n.ejs_env.slot_mapping[el] = i
                                i += 1
                                
                        
                        if n.ejs_env.parent?
                                parent_env_slot = n.ejs_env.slot_mapping[parent_env_name]
                                env_prepends.push b.expressionStatement(intrinsic setSlot_id, [this_env_id, b.literal(parent_env_slot), b.literal(parent_env_name), b.identifier(parent_env_name) ]);
                                

                        # we need to push assignments of any closed over parameters into the environment at this point
                        for param in n.params
                                if n.ejs_env.closed.has param.name
                                        env_prepends.push b.expressionStatement(intrinsic setSlot_id, [ this_env_id, b.literal(n.ejs_env.slot_mapping[param.name]), b.literal(param.name), b.identifier(param.name) ])

                        new_mapping["%slot_mapping"] = n.ejs_env.slot_mapping

                        flatten_memberexp = (exp, mapping) ->
                                if exp.type isnt CallExpression
                                        [b.literal(mapping[exp.name])]
                                else
                                        flatten_memberexp(exp.arguments[0], mapping).concat [exp.arguments[1]]

                        prepend_environment = (exps) ->
                                obj = this_env_id
                                for prop in exps
                                        obj = intrinsic(slot_id, [obj, prop])
                                obj

                        # if there are existing mappings prepend "%env." (a MemberExpression) to them
                        for mapped of new_mapping
                                val = new_mapping[mapped]
                                if mapped isnt "%slot_mapping"
                                        new_mapping[mapped] = prepend_environment(flatten_memberexp(val, n.ejs_env.slot_mapping))
                        
                        # and add mappings for all variables in .closed from "x" to "%env.x"

                        new_mapping["%env"] = this_env_id
                        n.ejs_env.closed.map (sym) ->
                                new_mapping[sym] = intrinsic slot_id, [this_env_id, b.literal(n.ejs_env.slot_mapping[sym]), b.literal(sym)]

                # remove all mappings for variables declared in this function
                if n.ejs_decls?
                        new_mapping = reject new_mapping, (sym) -> (n.ejs_decls.has sym) and not (n.ejs_env.closed.has sym)

                @mappings.push new_mapping
                @visitFunctionBody n
                if env_prepends.length > 0
                        n.body.body = env_prepends.concat n.body.body
                @mappings.pop()

                # convert function expressions to an explicit closure creation, so:
                # 
                #    function X () { ...body... }
                #
                # replace inline with:
                #
                #    makeClosure(%current_env, "X", function X () { ...body... })

                if not n.toplevel
                        if n.type is FunctionDeclaration
                                throw "there should be no FunctionDeclarations at this point"
                        else # n.type is FunctionExpression
                                intrinsic_args = []
                                intrinsic_args.push(if n.ejs_env.parent? then b.identifier(parent_env_name) else b.null())
                                
                                if n.id?
                                        intrinsic_id = makeClosure_id
                                        if n.id.type is Identifier
                                                intrinsic_args.push(b.literal(n.id.name))
                                        else
                                                intrinsic_args.push(b.literal(escodegen.generate n.id))
                                else
                                        intrinsic_id = makeAnonClosure_id

                                intrinsic_args.push(n)
                                
                                return intrinsic intrinsic_id, intrinsic_args
                n
            catch e
                console.warn "exception: " + e
                #console.warn "compiling the following code:"
                #console.warn escodegen.generate n
                throw e
                
        visitCallExpression: (n) ->
                n = super

                # replace calls of the form:
                #   X (arg1, arg2, ...)
                # 
                # with
                #   invokeClosure(X, %this, %argCount, arg1, arg2, ...);

                return n if is_intrinsic(n)
                @function_stack.top.scratch_size = Math.max @function_stack.top.scratch_size, n.arguments.length
                intrinsic invokeClosure_id, [n.callee].concat n.arguments

        visitNewExpression: (n) ->
                super

                # replace calls of the form:
                #   new X (arg1, arg2, ...)
                # 
                # with
                #   invokeClosure(X, %this, %argCount, arg1, arg2, ...);

                @function_stack.top.scratch_size = Math.max @function_stack.top.scratch_size, n.arguments.length

                rv = intrinsic invokeClosure_id, [n.callee].concat n.arguments
                rv.type = NewExpression
                rv

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

                n.params.unshift b.identifier("%env_#{n.ejs_env.parent.id}")
                
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

class MarkLocalAndGlobalVariables extends TransformPass
        constructor: (options, @filename) ->
                super
                # initialize the scope stack with the global (empty) scope
                @scope_stack = new Stack new Map

        findIdentifierInScope: (ident) ->
                for scope in @scope_stack.stack
                        return true if scope.has(ident.name)
                # at this point it's going to be a global reference.
                # make sure the identifier is one of our globals
                if hasOwnProperty.call runtime_globals, ident.name
                        return false

                if @options.warn_on_undeclared
                        reportWarning("undeclared identifier `#{ident.name}'", @filename, ident.loc)
                else
                        reportError(ReferenceError, "undeclared identifier `#{ident.name}'", @filename, ident.loc)
                false

        intrinsicForIdentifier: (id) ->
                is_local = @findIdentifierInScope id
                if is_local then getLocal_id else getGlobal_id
                
        visitWithScope: (scope, children) ->
                @scope_stack.push scope
                if Array.isArray children
                        rv = (@visit child for child in children)
                else
                        rv = @visit children
                @scope_stack.pop()
                rv

        visitVariableDeclarator: (n) ->
                @scope_stack.top.set(n.id.name, true)
                n

        visitImportSpecifier: (n) ->
                @scope_stack.top.set(n.id.name, true)
                n

        visitObjectExpression: (n) ->
                for property in n.properties
                        property.value = @visit property.value
                n

        visitAssignmentExpression: (n) ->
                lhs = n.left
                visited_rhs = @visit n.right
                
                if is_intrinsic lhs, "%slot"
                        intrinsic setSlot_id, [lhs.arguments[0], lhs.arguments[1], visited_rhs], lhs.loc
                else if lhs.type is Identifier
                        if @findIdentifierInScope lhs
                                intrinsic setLocal_id, [lhs, visited_rhs], lhs.loc
                        else
                                intrinsic setGlobal_id, [lhs, visited_rhs], lhs.loc
                else
                        n.left = @visit n.left
                        n.right = visited_rhs
                        n
                        
        visitBlock: (n) ->
                n.body = @visitWithScope(new Map(), n.body)
                n

        visitCallExpression: (n) ->
                # at this point there shouldn't be call expressions
                # for anything other than intrinsics, so we leave
                # callee alone and just iterate over the args.  we
                # skip the first one since that is guaranteed to be an
                # identifier that we don't want rewrapped, or an
                # intrinsic already wrapping the identifier.

                if n.arguments.length > 0
                        if n.arguments[0].type is Identifier
                                @findIdentifierInScope n.arguments[0]
                                new_args = @visit n.arguments.slice 1
                                new_args.unshift n.arguments[0]
                        else
                                new_args = @visit n.arguments
                        n.arguments = new_args
                n

        visitNewExpression: (n) ->
                # at this point there shouldn't be call expressions
                # for anything other than intrinsics, so we leave
                # callee alone and just iterate over the args.  we
                # skip the first one since that is guaranteed to be an
                # identifier that we don't want rewrapped, or an
                # intrinsic already wrapping the identifier.
                
                if n.arguments[0].type is Identifier
                        @findIdentifierInScope n.arguments[0]
                new_args = @visit n.arguments.slice 1
                new_args.unshift n.arguments[0]
                n.arguments = new_args
                n

        visitFunction: (n) ->
                new_scope = new Map
                new_scope.set(p.name, true) for p in n.params
                new_scope.set("arguments", true)
                new_body = @visitWithScope new_scope, n.body
                n.body = new_body
                n

        visitLabeledStatement: (n) ->
                n.body  = @visit n.body
                n

        visitCatchClause: (n) ->
                new_scope = new Map
                new_scope.set(n.param.name, true)
                n.guard = @visit n.guard
                n.body = @visitWithScope new_scope, n.body
                n

        visitIdentifier: (n) ->
                intrinsic @intrinsicForIdentifier(n), [n]

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
                isMakeClosure = (a) -> is_intrinsic(a, "%makeClosure") or is_intrinsic(a, "makeAnonClosure")
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
                for el in pattern.elements
                        memberexp = b.memberExpression(id, b.literal(el_num), true)

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
                                decls.push b.variableDeclarator(obj_tmp_id, decl.init)
                                createObjectPatternBindings(obj_tmp_id, decl.id, decls)
                        else if decl.id.type is ArrayPattern
                                # create a fresh tmp and declare it
                                array_tmp_id = fresh()
                                decls.push b.variableDeclarator(array_tmp_id, decl.init)
                                createArrayPatternBindings(array_tmp_id, decl.id, decls)
                                        
                        else if decl.id.type is Identifier
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
        exports_id = b.identifier("exports") # XXX as with compiler.coffee, this 'exports' should be '%exports' if the module has ES6 module declarations
        get_id = b.identifier("get")
        Object_id = b.identifier("Object")
        defineProperty_id = b.identifier("defineProperty")
        
        freshId = do ->
                importGen = startGenerator()
                (prefix) -> b.identifier "%#{prefix}_#{importGen()}"

        Object_defineProperty = b.memberExpression(Object_id, defineProperty_id)
                                
        define_export_property = (exported_id, local_id = exported_id) ->
                # return esprima.parse("Object.defineProperty(exports, '#{exported_id.name}', { get: function() { return #{local_id.name}; } });");
                
                getter = b.functionExpression(undefined, [], b.blockStatement([b.returnStatement(local_id)]))
                        
                property_literal = b.objectExpression([b.property(get_id, getter)])

                return b.expressionStatement(b.callExpression(Object_defineProperty, [exports_id, b.literal(exported_id.name), property_literal]))
                
        constructor: (options, @filename, @exportLists) ->
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
                        return b.expressionStatement(intrinsic(moduleGet_id, [n.source_path]))

                # otherwise create a fresh declaration for the module object
                # 
                # let %import_decl = %moduleGet("moduleName")
                # 
                import_tmp = freshId("import")
                import_decls =  b.letDeclaration(import_tmp, intrinsic(moduleGet_id, [n.source_path]))

                for spec in n.specifiers
                        if spec.kind is "default"
                                #
                                # let #{n.specifiers[0].id} = %import_decl.default
                                #
                                if not @exportLists[n.source_path.value]?.has_default
                                        reportError(ReferenceError, "module `#{n.source_path.value}' doesn't have default export", @filename, n.loc)

                                import_decls.declarations.push b.variableDeclarator(n.specifiers[0].id, b.memberExpression(import_tmp, b.identifier("default")))

                        else if spec.kind is "named"
                                #
                                # let #{spec.id} = %import_decl.#{spec.name || spec.id }
                                #
                                if not @exportLists[n.source_path.value]?.ids.has(spec.id.name)
                                        reportError(ReferenceError, "module `#{n.source_path.value}' doesn't export `#{spec.id.name}'", @filename, spec.id.loc)
                                import_decls.declarations.push b.variableDeclarator(spec.name or spec.id, b.memberExpression(import_tmp, spec.id))

                        else # if spec.kind is "batch"
                                # let #{spec.name} = %import_decl
                                import_decls.declarations.push b.variableDeclarator(spec.name, import_tmp)
                        
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
                        export_decl = b.letDeclaration(import_tmp, intrinsic(moduleGet_id, [n.source_path]))

                        export_stuff = []                             
                        if n.default
                                # export * from "foo"
                                if n.specifiers.length isnt 1 or n.specifiers[0].type isnt ExportBatchSpecifier
                                        reportError(SyntaxError, "invalid export", @filename, n.loc)
                                @batch_exports.push { source: import_tmp, specifiers: [] }
                        else
                                # export { ... } from "foo"
                                for spec in n.specifiers
                                        if not @exportLists[n.source_path.value]?.ids.has(spec.id.name)
                                                reportError(ReferenceError, "module `#{n.source_path.value}' doesn't export `#{spec.id.name}'", @filename, spec.id.loc)
                                        
                                        spectmp = freshId("spec")
                                        export_decl.declarations.push b.variableDeclarator(spectmp, b.memberExpression(import_tmp, spec.id))

                                        @exports.push { name: spec.name, id: spectmp }
                                        export_stuff.push(define_export_property(spec.name || spec.id, spectmp))
                        export_stuff.unshift(export_decl)
                        return export_stuff

                export_id = b.identifier("exports");
                
                # export function foo () { ... }
                if n.declaration.type is FunctionDeclaration
                        @exports.push { id: n.declaration.id }
                        
                        return [n.declaration, define_export_property(n.declaration.id)]

                # export class Foo () { ... }
                if n.declaration.type is ClassDeclaration
                        @exports.push { id: n.declaration.id }
                        
                        return [n.declaration, define_export_property(n.declaration.id)]

                # export let foo = bar;
                if n.declaration.type is VariableDeclaration
                        export_defines = []
                        for decl in n.declaration.declarations
                                @exports.push({ id: decl.id })
                                export_defines.push(define_export_property(decl.id))
                        export_defines.unshift(n.declaration)
                        return export_defines

                # export foo = bar;
                if n.declaration.type is VariableDeclarator
                        @exports.push { id: n.declaration.id }
                        return [n.declaration, define_export_property(n.declaration.id)]

                # export default ...;
                # 
                if n.default
                        local_default_id = b.identifier "%default"
                        default_id = b.identifier "default"
                        @exports.push { id: default_id }
                        
                        local_decl = b.varDeclaration(local_default_id, n.declaration)

                        export_define = define_export_property(default_id, local_default_id)
                        return [local_decl, export_define]

                reportError(SyntaxError, "Unsupported type of export declaration #{n.declaration.type}", @filename, n.loc)

        visitModuleDeclaration: (n) ->
                init = intrinsic(moduleGet_id, [n.source_path])
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
#     a = %argPresent(0) ? %getArg(0) : undefined;
#     b = %argPresent(1) ? %getArg(1) : a;
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
                        prepends.push(b.expressionStatement(b.assignmentExpression(p, '=', b.conditionalExpression(intrinsic(argPresent_id, [b.literal(i+1)]), intrinsic(getArg_id, [b.literal(i)]), d))))
                n.body.body = prepends.concat(n.body.body)
                n.defaults = []
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
        HoistVars
        NameAnonymousFunctions
        #CFA2 if enable_cfa2
        ComputeFree
        LocateEnv
        SubstituteVariables
        MarkLocalAndGlobalVariables
        IIFEIdioms
        LambdaLift
        ]

exports.convert = (tree, filename, export_lists, options) ->
        debug.log "before:"
        debug.log -> escodegen.generate tree

        passes.forEach (passType) ->
                return if not passType?
                try
                        pass = new passType(options, filename, export_lists)
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
