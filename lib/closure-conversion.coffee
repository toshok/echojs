esprima = require 'esprima'
escodegen = require 'escodegen'
debug = require 'debug'

{ CFA2 } = require 'jscfa2'

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
  create_intrinsic,
  is_intrinsic,
  create_identifier,
  create_string_literal,
  create_number_literal,
  startGenerator } = require 'echo-util'

hasOwnProperty = Object.prototype.hasOwnProperty

superid                 = create_identifier "%super"
makeClosureEnv_id       = create_identifier "%makeClosureEnv"
makeClosure_id          = create_identifier "%makeClosure"
makeAnonClosure_id      = create_identifier "%makeAnonClosure"
setSlot_id              = create_identifier "%setSlot"
slot_id                 = create_identifier "%slot"
invokeClosure_id        = create_identifier "%invokeClosure"
setLocal_id             = create_identifier "%setLocal"
setGlobal_id            = create_identifier "%setGlobal"
getLocal_id             = create_identifier "%getLocal"
getGlobal_id            = create_identifier "%getGlobal"
createArgScratchArea_id = create_identifier "%createArgScratchArea"

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
DesugarClasses = class DesugarClasses extends TreeVisitor
        constructor: ->
                super
                @class_stack = new Stack
                @method_stack = new Stack

        visitCallExpression: (n) ->
                if n.callee.type is Identifier and n.callee.name is "super"
                        n.callee =
                                type: MemberExpression
                                object:
                                        type: MemberExpression
                                        object: superid
                                        property: create_identifier "prototype"
                                        computed: false
                                property: @method_stack.top
                                computed: false
                n.arguments = @visitArray n.arguments
                n

        visitNewExpression: (n) ->
                if n.callee.type is Identifier and n.callee.name is "super"
                        n.callee = superid
                n.arguments = @visitArray n.arguments
                n
                
        visitIdentifier: (n) ->
                return superid if n.name is "super"
                n

        visitClassDeclaration: (n) ->
                # we visit all the functions defined in the class so that 'super' is replaced with '%super'
                @class_stack.push n

                # XXX this push/pop should really be handled in @visitMethodDefinition
                for class_element in n.body.body
                        @method_stack.push class_element.key
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
                                class_init_iife_body.push @create_constructor m, n
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
                #   function Subclass (...args) { _super.call(this, args...); }
                if not ctor?
                        class_init_iife_body.unshift @create_default_constructor n

                # make sure we return the function from our iife
                class_init_iife_body.push
                        type: ReturnStatement
                        argument: n.id

                # this block forms the outer wrapper iife + initializer:
                # 
                #  let %className = (function (%super?) { ... })(%superClassName);
                #
                class_init = 
                        type: VariableDeclaration
                        kind: "let"
                        declarations: [{
                                type: VariableDeclarator
                                id:   n.id
                                init:
                                        type: CallExpression
                                        callee:
                                                type: FunctionExpression
                                                id:   null
                                                body: 
                                                        type: BlockStatement
                                                        body: class_init_iife_body
                                                params: if n.superClass? then [superid] else []
                                                defaults: []
                                                rest: null
                                                generator: false
                                                expression: false
                                        arguments: if n.superClass? then [n.superClass] else []
                        }]
                        
                class_init

        gather_members: (ast_class) ->
                methods     = new Map
                smethods    = new Map
                properties  = new Map
                sproperties = new Map

                for class_element in ast_class.body.body
                        if class_element.static and class_element.key.name is "prototype"
                                throw new SyntaxError "Illegal method name 'prototype' on static class member."
                                
                        if class_element.kind is ""
                                # a method
                                method_map = if class_element.static then smethods else methods
                                throw new SyntaxError "method '#{class_element.key.name}' has already been defined." if method_map.has(class_element.key.name)
                                method_map.set(class_element.key.name, class_element)
                        else
                                # a property
                                property_map = if class_element.static then sproperties else properties
                                
                                property_map.set(class_element.key.name, new Map) if not property_map.has(class_element.key.name)

                                throw new SyntaxError "a '#{class_element.kind}' method for '#{class_element.key.name}' has already been defined." if property_map.get(class_element.key.name).has(class_element.kind)
                                property_map.get(class_element.key.name).set(class_element.kind, class_element)

                [properties, methods, sproperties, smethods]
                        
                
        create_constructor: (ast_method, ast_class) ->
                return {
                        type: FunctionDeclaration
                        id: ast_class.id
                        body: ast_method.value.body
                        params: ast_method.value.params
                        defaults: ast_method.value.defaults
                        rest: ast_method.value.rest
                        generator: false
                        expression: false
                }

        create_default_constructor: (ast_class) ->
                return {
                        type: FunctionDeclaration
                        id: ast_class.id
                        body:
                                # XXX splat args into the call to super's ctor
                                type: BlockStatement
                                body: []
                        params: []
                        defaults: []
                        rest: create_identifier "args"
                        generator: false
                        expression: false
                }
                
        create_proto_method: (ast_method, ast_class) ->
                proto_member =
                        type: MemberExpression
                        object:
                                type: MemberExpression
                                object: ast_class.id
                                property: create_identifier "prototype"
                                computed: false
                        property: ast_method.key
                        computed: false

                method =
                        type: FunctionExpression
                        id: ast_method.key
                        body: ast_method.value.body
                        params: ast_method.value.params
                        defaults: []
                        rest: null
                        generator: false
                        expression: false
                        
                
                return {
                        type: ExpressionStatement
                        expression:
                                type: AssignmentExpression
                                operator: "="
                                left: proto_member
                                right: method
                }

        create_static_method: (ast_method, ast_class) ->
                method =
                        type: FunctionExpression
                        id: ast_method.key
                        body: ast_method.value.body
                        params: ast_method.value.params
                        defaults: []
                        rest: null
                        generator: false
                        expression: false
                        
                
                return {
                        type: ExpressionStatement
                        expression:
                                type: AssignmentExpression
                                operator: "="
                                left:
                                        type: MemberExpression
                                        object: ast_class.id
                                        property: ast_method.key
                                        computed:false
                                right: method
                }

        create_properties: (properties, ast_class, are_static) ->
                propdescs = []

                properties.forEach (prop_map, prop) =>
                        accessors = []
                        name = null

                        getter = prop_map.get("get")
                        setter = prop_map.get("set")

                        if getter?
                                accessors.push { type: Property, key: create_identifier("get"), value: getter.value, kind: "init" }
                                name = prop
                        if prop.set?
                                accessors.push { type: Property, key: create_identifier("set"), value: setter.value, kind: "init" }
                                name = prop

                        propdescs.push
                                type: Property
                                key: create_identifier name
                                value:
                                        type: ObjectExpression
                                        properties: accessors
                                kind: "init"

                return null if propdescs.length is 0

                propdescs_literal =
                        type: ObjectExpression
                        properties: propdescs

                if are_static
                        target = ast_class.id
                else
                        target =
                                type: MemberExpression
                                object: ast_class.id
                                property: create_identifier "prototype"
                                computed: false

                return {
                        type: ExpressionStatement
                        expression:
                                type: CallExpression
                                callee:
                                        type: MemberExpression
                                        object: create_identifier "Object"
                                        property: create_identifier "defineProperties"
                                arguments: [target, propdescs_literal]
                }

#
# each element in env is an object of the form:
#   { exp: ...     // the AST node corresponding to this environment.  will be a function or a BlockStatement. (right now just functions)
#     id:  ...     // the number/id of this environment
#     decls: ..    // a set of the variable names that are declared in this environment
#     closed: ...  // a set of the variable names that are used from this environment (i.e. the ones that need to move to the environment)
#     parent: ...  // the parent environment object
#   }
# 
LocateEnv = class LocateEnv extends TreeVisitor
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
                # we don't visit property keys here
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

class HoistFuncDecls extends TreeVisitor
        constructor: ->
                @prepends = []
                
        visitFunction: (n) ->
                @prepends.unshift null
                n.body = @visit n.body
                # we're assuming n.body is a BlockStatement here...
                new_prepends = @prepends.shift()
                if new_prepends isnt null
                        n.body.body = new_prepends.concat n.body.body
                n
        
        visitBlock: (n) ->
                n = super n
                return n if n.body.length == 0

                i = 0
                e = n.body.length
                while i < e
                        child = n.body[i]
                        if child.type is FunctionDeclaration
                                if @prepends[0] is null
                                        @prepends[0] = []
                                @prepends[0].push child
                                n.body.splice i, 1
                                e = n.body.length
                        else
                                i += 1
                n

# convert all function declarations to variable assignments
# with named function expressions.
# 
# i.e. from:
#   function foo() { }
# to:
#   var foo = function foo() { }
# 
class FuncDeclsToVars extends TreeVisitor
        constructor: -> super
        
        visitFunctionDeclaration: (n) ->
                if n.toplevel
                        super
                else
                        func_exp = n
                        func_exp.type = FunctionExpression
                        func_exp.body = @visit func_exp.body
                        return {
                                type: VariableDeclaration,
                                declarations: [{
                                        type: VariableDeclarator
                                        id: create_identifier n.id.name
                                        init: func_exp
                                }],
                                kind: "var"
                        }


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
class HoistVars extends TreeVisitor
        constructor: () ->
                super
                @scope_stack = new Stack

        create_empty_declarator = (decl_name) ->
                type: VariableDeclarator
                id: create_identifier decl_name
                init: null

                
        visitProgram: (n) ->
                vars = new Set
                @scope_stack.push { func: n, vars: vars }
                n = super
                @scope_stack.pop()

                return n if vars.size() is 0

                n.body.unshift
                        type: VariableDeclaration
                        declarations: create_empty_declarator varname for varname in vars.keys()
                        kind: "let"
                n
        
        visitFunction: (n) ->
                vars = new Set
                @scope_stack.push { func: n, vars: vars }
                n = super
                @scope_stack.pop()

                return n if vars.size() is 0

                n.body.body.unshift
                        type: VariableDeclaration
                        declarations: create_empty_declarator varname for varname in vars.keys()
                        kind: "let"
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
                        n.left = create_identifier n.left.declarations[0].id.name
                n.right = @visit n.right
                n.body = @visit n.body
                n

        visitForOf: (n) ->
                if n.left.type is VariableDeclaration
                        @scope_stack.top.vars.add n.left.declarations[0].id.name
                        n.left = create_identifier n.left.declarations[0].id.name
                n.right = @visit n.right
                n.body = @visit n.body
                n
                                                
        visitVariableDeclaration: (n) ->
                if n.kind is "var"
                        # check to see if there are any initializers, which we'll convert to assignment expressions
                        assignments = []
                        for i in [0...n.declarations.length]
                                if n.declarations[i].init?
                                        assignment =
                                                type: AssignmentExpression
                                                left: create_identifier n.declarations[i].id.name
                                                right: @visit n.declarations[i].init
                                                operator: "="

                                        assignments.push assignment

                        # vars are hoisted to the containing function's toplevel scope
                        @scope_stack.top.vars.add decl.id.name for decl in n.declarations

                        if assignments.length is 0
                                return { type: EmptyStatement }
                                
                        # now return the new assignments, which will replace the original variable
                        # declaration node.
                        if assignments.length > 1
                                assign_exp =
                                        type: SequenceExpression
                                        expressions: assignments
                        else
                                assign_exp = assignments[0]


                        if @skipExpressionStatement
                                return assign_exp
                        else
                                return {
                                        type: ExpressionStatement
                                        expression: assign_exp
                                }
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
DesugarArrowFunctions = class DesugarArrowFunctions extends TreeVisitor
        definesThis: (n) -> n.type is FunctionDeclaration or n.type is FunctionExpression
        
        constructor: ->
                @mapping = []
                @thisGen = startGenerator()

        visitArrowFunctionExpression: (n) ->
                if n.expression
                        n.body = {
                                type: BlockStatement
                                body: [{
                                        type: ReturnStatement
                                        argument: n.body
                                }]
                        }
                        n.expression = false
                n = @visitFunction n
                n.type = FunctionExpression
                n

        visitThisExpression: (n) ->
                return { type: Literal, value: undefined } if @mapping.length == 0

                topfunc = @mapping[0].func

                for m in @mapping
                        if @definesThis m.func
                                # if we're already on top, just return the existing thisExpression
                                return n if topfunc is m

                                return create_identifier m.id if m.id?

                                m.id = "_this_#{@thisGen()}"

                                m.prepend = {
                                        type: VariableDeclaration
                                        declarations: [{
                                                type: VariableDeclarator
                                                id:  create_identifier m.id
                                                init:
                                                        type: ThisExpression
                                        }],
                                        kind: "let"
                                }

                                return create_identifier m.id

                throw new Error("no binding for 'this' available for arrow function")
        
        visitFunction: (n) ->
                @mapping.unshift { func: n, id: null }
                n = super n
                m = @mapping.shift()
                if m.prepend?
                        n.body.body.unshift m.prepend
                n
                

# this class really doesn't behave like a normal TreeVisitor, as it modifies the tree in-place.
# XXX reformulate this as a TreeVisitor subclass.
class ComputeFree extends TreeVisitor
        constructor: ->
                @call_free = @free.bind @
                @setUnion = Set.union
                super
                
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
                decls = @decl_names @collect_decls body
                uses = @setUnion.apply null, body.map @call_free
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
                        when UpdateExpression      then exp.ejs_free_vars = @free exp.argument
                        when ReturnStatement       then exp.ejs_free_vars = @free exp.argument
                        when UnaryExpression       then exp.ejs_free_vars = @free exp.argument
                        when BinaryExpression      then exp.ejs_free_vars = @free(exp.left).union @free exp.right
                        when LogicalExpression     then exp.ejs_free_vars = @free(exp.left).union @free exp.right
                        when MemberExpression      then exp.ejs_free_vars = @free exp.object # we don't traverse into the property
                        when CallExpression        then exp.ejs_free_vars = @setUnion.apply null, [@free exp.callee].concat exp.arguments.map @call_free
                        when NewExpression         then exp.ejs_free_vars = @setUnion.apply null, [@free exp.callee].concat exp.arguments.map @call_free
                        when SequenceExpression    then exp.ejs_free_vars = @setUnion.apply null, exp.expressions.map @call_free
                        when ConditionalExpression then exp.ejs_free_vars = @setUnion.call null, @free(exp.test), @free(exp.consequent), @free(exp.alternate)
                        when Literal               then exp.ejs_free_vars = new Set
                        when ThisExpression        then exp.ejs_free_vars = new Set
                        when Property              then exp.ejs_free_vars = @free exp.value # we skip the key
                        when ObjectExpression
                                exp.ejs_free_vars = if exp.properties.length is 0 then (new Set) else @setUnion.apply null, (@free p.value for p in exp.properties)
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
class SubstituteVariables extends TreeVisitor
        constructor: (module) ->
                @function_stack = new Stack
                @mappings = new Stack
                super

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
                        return {
                                type: BlockStatement
                                body: init.concat [n]
                        }
                else
                        n.init = init
                        n

        visitForIn: (n) ->
                # for-in loops complicate things.

                left = @visit n.left
                n.right = @visit n.right
                n.body = @visit n.body
                if Array.isArray left
                        console.log "whu?"
                        n.left = create_identifier left[0].declarations[0].id.name
                        return {
                                type: BlockStatement
                                body: left.concat [n]
                        }
                else
                        n.left = left
                        n

        visitForOf: (n) ->
                n.left  = @visit n.left
                n.right = @visit n.right
                n.body  = @visit n.body
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
                                                rv.push {
                                                        type: VariableDeclaration
                                                        declarations: new_declarations
                                                        kind: n.kind
                                                }

                                        # splice in this assignment
                                        rv.push {
                                                type: ExpressionStatement
                                                expression: 
                                                        type: AssignmentExpression
                                                        left: @currentMapping()[decl.id.name]
                                                        right: decl.init
                                                        operator: "="
                                        }

                                        # then re-init the new_declarations array
                                        new_declarations = []
                        
                        else
                                # for variables that aren't closed over, we just add them to the currect decl list.
                                new_declarations.push decl
                        

                # push the last set of new_declarations if there were any
                if new_declarations.length > 0
                        rv.push {
                                type: VariableDeclaration
                                declarations: new_declarations
                                kind: n.kind
                        }

                if rv.length is 0
                        rv = { type: EmptyStatement }
                rv

        visitProperty: (n) ->
                # we don't visit property keys here
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
                this_env_name = "%env_#{n.ejs_env.id}"
                if n.ejs_env.parent?
                        parent_env_name = "%env_#{n.ejs_env.parent.id}"

                env_prepends = []
                new_mapping = shallow_copy_object @currentMapping()
                if n.ejs_env.closed.empty() and not n.ejs_env.nested_requires_env
                        env_prepends.push {
                                type: VariableDeclaration,
                                declarations: [{
                                        type: VariableDeclarator
                                        id:  create_identifier this_env_name
                                        init:
                                                type: Literal
                                                value: null
                                }],
                                kind: "let"
                        }
                else
                        # insert environment creation (at the start of the function body)
                        env_prepends.push {
                                type: VariableDeclaration,
                                declarations: [{
                                        type: VariableDeclarator
                                        id:  create_identifier this_env_name
                                        init: create_intrinsic makeClosureEnv_id, [create_number_literal n.ejs_env.closed.size() + if n.ejs_env.parent? then 1 else 0]
                                }],
                                kind: "let"
                        }

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
                                env_prepends.push {
                                        type: ExpressionStatement
                                        expression: create_intrinsic setSlot_id, [create_identifier(this_env_name), create_number_literal(parent_env_slot), create_string_literal(parent_env_name), create_identifier(parent_env_name) ]
                                }
                                

                        # we need to push assignments of any closed over parameters into the environment at this point
                        for param in n.params
                                if n.ejs_env.closed.has param.name
                                        env_prepends.push {
                                                type: ExpressionStatement
                                                expression: create_intrinsic setSlot_id, [ create_identifier(this_env_name), create_number_literal(n.ejs_env.slot_mapping[param.name]), create_string_literal(param.name), create_identifier(param.name) ]
                                        }

                        new_mapping["%slot_mapping"] = n.ejs_env.slot_mapping

                        flatten_memberexp = (exp, mapping) ->
                                if exp.type isnt CallExpression
                                        [create_number_literal(mapping[exp.name])]
                                else
                                        flatten_memberexp(exp.arguments[0], mapping).concat [exp.arguments[1]]

                        prepend_environment = (exps) ->
                                obj = create_identifier(this_env_name)
                                for prop in exps
                                        obj = create_intrinsic(slot_id, [ obj, prop ])
                                obj

                        # if there are existing mappings prepend "%env." (a MemberExpression) to them
                        for mapped of new_mapping
                                val = new_mapping[mapped]
                                if mapped isnt "%slot_mapping"
                                        new_mapping[mapped] = prepend_environment(flatten_memberexp(val, n.ejs_env.slot_mapping))
                        
                        # and add mappings for all variables in .closed from "x" to "%env.x"

                        new_mapping["%env"] = create_identifier this_env_name
                        n.ejs_env.closed.map (sym) ->
                                new_mapping[sym] = create_intrinsic slot_id, [ (create_identifier this_env_name), (create_number_literal n.ejs_env.slot_mapping[sym]), (create_string_literal sym) ]

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
                                if n.id?
                                        return create_intrinsic makeClosure_id, [ (if n.ejs_env.parent? then (create_identifier parent_env_name) else { type: Literal, value: null}), (create_string_literal n.id.name), n ]
                                else
                                        return create_intrinsic makeAnonClosure_id, [ (if n.ejs_env.parent? then (create_identifier parent_env_name) else { type: Literal, value: null}), n ]

                n
            catch e
                console.warn "exception: " + e
                console.warn "compiling the following code:"
                #console.warn escodegen.generate n
                throw e
                
        visitCallExpression: (n) ->
                super

                # replace calls of the form:
                #   X (arg1, arg2, ...)
                # 
                # with
                #   invokeClosure(X, %this, %argCount, arg1, arg2, ...);

                @function_stack.top.scratch_size = Math.max @function_stack.top.scratch_size, n.arguments.length
                create_intrinsic invokeClosure_id, [n.callee].concat n.arguments

        visitNewExpression: (n) ->
                super

                # replace calls of the form:
                #   new X (arg1, arg2, ...)
                # 
                # with
                #   invokeClosure(X, %this, %argCount, arg1, arg2, ...);

                @function_stack.top.scratch_size = Math.max @function_stack.top.scratch_size, n.arguments.length

                rv = create_intrinsic invokeClosure_id, [n.callee].concat n.arguments
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
class LambdaLift extends TreeVisitor
        constructor: (@filename) ->
                @functions = []
                super

        visitProgram: (program) ->
                super
                program.body = @functions.concat program.body
                program

        maybePrependScratchArea: (n) ->
                if n.scratch_size > 0
                        alloc_scratch =
                                type: ExpressionStatement
                                expression: create_intrinsic createArgScratchArea_id, [ { type: Literal, value: n.scratch_size } ]
                        n.body.body.unshift alloc_scratch
                
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
                n.id =
                        type: Identifier
                        name: global_name

                @functions.push n

                n.body = @visit n.body

                @maybePrependScratchArea n

                n.params.unshift create_identifier "%env_#{n.ejs_env.parent.id}"
                
                return {
                        type: Identifier,
                        name: global_name
                }


class NameAnonymousFunctions extends TreeVisitor
        constructor: -> super
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

class MarkLocalAndGlobalVariables extends TreeVisitor
        constructor: ->
                # initialize the scope stack with the global (empty) scope
                @scope_stack = new Stack new Map

        findIdentifierInScope: (ident) ->
                for scope in @scope_stack.stack
                        return true if scope.has(ident.name)
                return false

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

        # we split up assignment operators +=/-=/etc into their
        # component operator + assignment so we can mark lhs as
        # setLocal/setGlobal/etc, and rhs getLocal/getGlobal/etc
        visitAssignmentExpression: (n) ->
                lhs = n.left
                rhs = @visit n.right
                if n.operator.length is 1
                        new_rhs = rhs
                else
                        new_rhs = {
                                type:     BinaryExpression,
                                operator: n.operator[0]
                                left:     lhs
                                right:    rhs
                        }
                        n.operator = '='
                
                if is_intrinsic "%slot", lhs
                        create_intrinsic setSlot_id, [lhs.arguments[0], lhs.arguments[1], new_rhs]
                else if lhs.type is Identifier
                        if @findIdentifierInScope lhs
                                create_intrinsic setLocal_id, [lhs, new_rhs]
                        else
                                create_intrinsic setGlobal_id, [lhs, new_rhs]
                else
                        n.left = @visit n.left
                        n.right = new_rhs
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

                if n.arguments[0].type is Identifier
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
                
                new_args = @visit n.arguments.slice 1
                new_args.unshift n.arguments[0]
                n.arguments = new_args
                n

        visitFunction: (n) ->
                new_scope = new Map
                new_scope.set(p.name, true) for p in n.params
                new_scope.set(n.rest.name, true) if n.rest?
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
                create_intrinsic @intrinsicForIdentifier(n), [n]

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
                @function_stack = new Stack
                @iife_generator = startGenerator()
                super

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
                iife_rv_id = create_identifier "%iife_rv_#{@iife_generator()}"

                replacement = { type: BlockStatement, body: [] }

                replacement.body.push {
                        type: VariableDeclaration,
                        kind: "let",
                        declarations: [{
                                type: VariableDeclarator
                                id:   iife_rv_id
                                init: undefined
                        }]
                }

                for i in [0...arity]
                        replacement.body.push {
                                type: VariableDeclaration,
                                kind: "let",
                                declarations: [{
                                        type: VariableDeclarator
                                        id:   candidate.arguments[0].arguments[1].params[i]
                                        init: if i <= arg_count then candidate.arguments[i+1] else undefined
                                }]
                        }
                        
                @function_stack.top.scratch_size = Math.max @function_stack.top.scratch_size, candidate.arguments[0].arguments[1].scratch_size

                body = candidate.arguments[0].arguments[1].body
                body.ejs_iife_rv = iife_rv_id
                body.fromIIFE = true

                replacement.body.push body

                if is_intrinsic "%setSlot", n.expression
                        n.expression.arguments[2] = create_intrinsic getLocal_id, [iife_rv_id]
                        replacement.body.push n
                else if (is_intrinsic "%setGlobal", n.expression) or (is_intrinsic "setLocal", n.expression)
                        n.expression.arguments[1] = create_intrinsic getLocal_id, [iife_rv_id]
                        replacement.body.push n
                        
                return replacement

        maybeInlineIIFECall: (candidate, n) ->
                return n if candidate.arguments.length isnt 2 or candidate.arguments[1].type isnt ThisExpression

                iife_rv_id = create_identifier "%iife_rv_#{@iife_generator()}"

                replacement = { type: BlockStatement, body: [] }

                replacement.body.push {
                        type: VariableDeclaration,
                        kind: "let",
                        declarations: [{
                                type: VariableDeclarator
                                id:   iife_rv_id
                                init: undefined
                        }]
                }

                body = candidate.arguments[0].object.arguments[1].body
                body.ejs_iife_rv = iife_rv_id
                body.fromIIFE = true

                replacement.body.push body

                if is_intrinsic "%setSlot", n.expression
                        n.expression.arguments[2] = create_intrinsic getLocal_id, [iife_rv_id]
                        replacement.body.push n
                else if (is_intrinsic "%setGlobal", n.expression) or (is_intrinsic "setLocal", n.expression)
                        n.expression.arguments[1] = create_intrinsic getLocal_id, [iife_rv_id]
                        replacement.body.push n

                return replacement
        
        visitExpressionStatement: (n) ->
                isMakeClosure = (a) -> (is_intrinsic "%makeClosure", a) or (is_intrinsic "makeAnonClosure", a)
                # bail out early if we know we aren't in the right place
                if is_intrinsic "%invokeClosure", n.expression
                        candidate = n.expression
                else if is_intrinsic "%setSlot", n.expression
                        candidate = n.expression.arguments[2]
                else if (is_intrinsic "%setGlobal", n.expression) or (is_intrinsic "%setLocal", n.expression)
                        candidate = n.expression.arguments[1]
                else
                        return n

                # at this point candidate should only be an invokeClosure intrinsic
                return n if not is_intrinsic "%invokeClosure", candidate

                if isMakeClosure candidate.arguments[0]
                        return @maybeInlineIIFE candidate, n
                else if candidate.arguments[0].type is MemberExpression and (isMakeClosure candidate.arguments[0].object) and candidate.arguments[0].property.name is "call"
                        return @maybeInlineIIFECall candidate, n
                else
                        return n
                        
class DesugarDestructuring extends TreeVisitor
        gen = startGenerator()
        fresh = () -> create_identifier "destruct_tmp#{gen()}"

        # given an assignment { pattern } = id
        # 
        createObjectPatternBindings = (id, pattern, decls) ->
                for prop in pattern.properties
                        memberexp = {
                                type:     MemberExpression
                                object:   id
                                property: prop.key
                        }
                        
                        decls.push {
                                type: VariableDeclarator
                                id:   prop.key
                                init: memberexp
                        }
                        
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
                        memberexp = {
                                type:     MemberExpression
                                object:   id
                                computed: true
                                property: create_number_literal(el_num)
                        }

                        if el.type is Identifier
                                decls.push {
                                        type: VariableDeclarator
                                        id:   el
                                        init: memberexp
                                }
                        else if el.type is ObjectPattern
                                p_id = fresh()

                                decls.push {
                                        type: VariableDeclarator
                                        id:   p_id
                                        init: memberexp
                                }
                                createObjectPatternBindings(p_id, el, decls)
                        else if el.type is ArrayPattern
                                p_id = fresh()

                                decls.push {
                                        type: VariableDeclarator
                                        id:   p_id
                                        init: memberexp
                                }
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
                                new_decl =
                                        type: VariableDeclaration
                                        kind: "let"
                                        declarations: []
                                createObjectPatternBindings(p_id, p, new_decl.declarations)
                                new_decls.push new_decl
                        else if p.type is ArrayPattern
                                p_id = fresh()
                                new_params.push p_id
                                new_decl =
                                        type: VariableDeclaration
                                        kind: "let"
                                        declarations: []
                                createArrayPatternBindings(p_id, p, new_decl.declarations)
                                new_decls.push new_decl
                        else if p.type is Identifier
                                # we just pass this along
                                new_params.push(p)
                        else
                                throw new Error "unhandled type of formal parameter in DesugarDestructuring #{p.type}"

                n.body.body = new_decls.concat(n.body.body)
                n.params = new_params
                super

        visitVariableDeclaration: (n) ->
                decls = []

                for decl in n.declarations
                        if decl.id.type is ObjectPattern
                                obj_tmp_id = fresh()
                                decls.push
                                        type: VariableDeclarator
                                        id: obj_tmp_id
                                        init: decl.init
                                createObjectPatternBindings(obj_tmp_id, decl.id, decls)
                        if decl.id.type is ArrayPattern
                                # create a fresh tmp and declare it
                                array_tmp_id = fresh()
                                decls.push
                                        type: VariableDeclarator
                                        id: array_tmp_id
                                        init: decl.init
                                createArrayPatternBindings(array_tmp_id, decl.id, decls)
                                        
                        else if decl.id.type is Identifier
                                decls.push(decl)
                        else
                                throw new Error "unhandled type of variable declaration in DesugarDestructuring #{decl.id.type}"

                n.declarations = decls
                n

        visitAssignmentExpression: (n) ->
                if n.left.type is ObjectPattern or n.left.type is ArrayPattern
                        throw new Error "EJS doesn't support destructuring assignments yet (issue #16)"
                        throw new SyntaxError "cannot use destructuring with assignment operators other than '='" if n.operator isnt "="
                else
                        n

# switch this to true if you want to experiment with the new CFA2
# code.  definitely, definitely not ready for prime time.
enable_cfa2 = false

passes = [
        DesugarClasses
        DesugarDestructuring
        HoistFuncDecls
        FuncDeclsToVars
        HoistVars
        DesugarArrowFunctions
        NameAnonymousFunctions
        CFA2 if enable_cfa2
        ComputeFree
        LocateEnv
        SubstituteVariables
        MarkLocalAndGlobalVariables
        IIFEIdioms
        LambdaLift
        ]

exports.convert = (tree, filename, options) ->
        debug.log "before:"
        debug.log -> escodegen.generate tree

        passes.forEach (passType) ->
                return if not passType?
                try
                        pass = new passType(filename)
                        tree = pass.visit tree
                        if options.debug_passes.has (passType.name)
                                console.log "after: #{passType.name}"
                                console.log escodegen.generate tree

                        debug.log 2, "after: #{passType.name}"
                        debug.log 2, -> escodegen.generate tree
                        debug.log 3, -> __ejs.GC.dumpAllocationStats "after #{passType.name}" if __ejs?
                catch e
                        console.warn "exception in pass #{passType.name}"
                        console.warn e
                        throw e

        tree
