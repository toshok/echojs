esprima = require 'esprima'
escodegen = require 'escodegen'
debug = require 'debug'

b = require 'ast-builder'

{ reportError, reportWarning } = require 'errors'

runtime_globals = require('runtime').createGlobalsInterface(null)

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

slotIntrinsic          = (name, slot)        -> intrinsic(slot_id, [b.identifier(name), b.literal(slot)])
setSlotIntrinsic       = (name, slot, value) ->
        try
                intrinsic(setSlot_id, [b.identifier(name), b.literal(slot), value])
        catch e
                console.log "invalid setSlot intrinsic:"
                console.log "name = #{name}"
                console.log "slot = #{slot}"
                console.log "value = #{JSON.stringify value}"
                

is_getset_intrinsic = (n) ->
        return false if not is_intrinsic(n)
        return true if n.callee.name is slot_id.name    or n.callee.name is getLocal_id.name or n.callee.name is getGlobal_id.name
        return true if n.callee.name is setSlot_id.name or n.callee.name is setLocal_id.name or n.callee.name is setGlobal_id.name
        false

class TransformPass extends TreeVisitor
        constructor: (@options) ->

class Location
        constructor: (@block, @func) ->

class Scope
        scope_id = 0

        constructor: (@location) ->
                @bindings = new Map    # the identifiers declared in this scope, mapping from string(name) -> Binding
                @referents = new Map   # the references (rooted in other scopes) to identifiers declared in this scope, mapping from string(name) -> [Reference]
                @references = new Map  # the references rooted in this scope, mapping from string(name) -> [Reference]
                @scope_id = scope_id
                @parentScope = null
                scope_id += 1
                
        addBinding: (binding) ->
                @bindings.set(binding.name, binding)
                binding.declaringScope = @
        getBinding: (name) -> @bindings.get(name)
        hasBinding: (name) -> @bindings.has(name)

        addReferent: (ref) ->
                reflist = @referents.get(ref.binding.name)
                if not reflist
                        reflist = []
                        @referents.set(ref.binding.name, reflist)
                reflist.push(ref)

        getReferents: (name) -> @referents.get(name)
        hasReferents: (name) -> @referents.has(name)
                
        addReference: (ref) ->
                @references.set(ref.binding.name, ref)
                ref.referencingScope = @
                if ref.binding.type is 'local' or ref.binding.type is 'arg'
                        ref.binding.declaringScope.addReferent(ref)

        getReference: (name) -> @references.get(name)
        hasReference: (name) -> @references.has(name)

        isAncestorOf: (s) ->
                _s = @parentScope
                while _s?
                        return true if _s is s
                        _s = _s.parentScope
                false
                
                
class Binding
        constructor: (@name, @type, @is_const) ->

class LocalBinding extends Binding
        constructor: (name, is_const) ->
                super(name, 'local', is_const)

class GlobalBinding extends Binding
        constructor: (name, is_const) ->
                super(name, 'global', is_const)

class Reference
        constructor: (@binding) ->

class Environment
        constructor: (@name, @scope) ->
                @slot_map = new Map

        hasSlot: (name) -> @slot_map.has(name)
        getSlot: (name) -> @slot_map.get(name)
        addSlot: (name) ->
                return if @slot_map.has(name)
                @slot_map.set(name, @slot_map.size())

        
class ClosureConvert extends TransformPass
        
allFunctions = []

global_bindings = new Map

SubstituteVariables = class SubstituteVariables extends TransformPass
        constructor: (options, @filename) ->
                @options = options
                @current_scope = null
                
        visitBlock: (n) ->
                @current_scope = n.scope
                super(n)
                @current_scope = @current_scope.parentScope
                n

        env_name: -> @current_scope.env.name
        env_slot: (name) -> @current_scope.env.getSlot(name)

        visitVariableDeclaration: (n) ->
                throw new Error("VariableDeclarations should only have 1 declarator at this point") if n.declarations.length > 1
                decl = n.declarations[0]

                decl.init = @visit(decl.init)
                # don't visit the id

                referents = @current_scope.getReferents(decl.id.name)
                if referents?
                        for referent in referents
                                if referent.referencingScope.location.func isnt @current_scope.location.func
                                        # it's closed over, so we need to set it in our allocated environment
                                        return b.expressionStatement(setSlotIntrinsic(@env_name(), @env_slot(decl.id.name), decl.init or b.undefined()))
                return n

        visitCallExpression: (n) ->
                # if it's one of our get/set Slot/Local/Global intrinsics, bail
                return n if is_getset_intrinsic(n)

                # otherwise we need to visit the args
                if is_intrinsic(n)
                        n.arguments = @visit n.arguments
                        return n
                        
                n = super(n)
                @current_scope.location.func.scratch_size = Math.max @current_scope.location.func.scratch_size, n.arguments.length
                intrinsic(invokeClosure_id, [n.callee].concat(n.arguments))

        visitNewExpression: (n) ->
                n = super

                # replace calls of the form:
                #   new X (arg1, arg2, ...)
                # 
                # with
                #   invokeClosure(X, %this, %argCount, arg1, arg2, ...);

                @current_scope.location.func.scratch_size = Math.max @current_scope.location.func.scratch_size, n.arguments.length

                rv = intrinsic(invokeClosure_id, [n.callee].concat(n.arguments))
                rv.type = NewExpression
                rv

        visitFunction: (n) ->
                n.scratch_size = 0
                n.body = @visit n.body

                return n if n.toplevel
                        
                if n.type is FunctionDeclaration
                        throw new Error("there should be no FunctionDeclarations at this point")

                intrinsic_args = [b.identifier(n.params[0].name)]
                        
                if n.id?
                        intrinsic_id = makeClosure_id
                        if n.id.type is Identifier
                                intrinsic_args.push(b.literal(n.id.name))
                        else
                                intrinsic_args.push(b.literal(escodegen.generate n.id))
                else
                        intrinsic_id = makeAnonClosure_id

                intrinsic_args.push(n)
                        
                intrinsic(intrinsic_id, intrinsic_args)
                                

        visitAssignmentExpression: (n) ->
                return super(n) if n.left.type isnt Identifier

                rhs = @visit n.right
                leftname = n.left.name
                
                referents = @current_scope.getReferents(leftname)
                if referents?
                        for referent in referents
                                if referent.referencingScope.location.func isnt @current_scope.location.func
                                        # it's closed over, so we need to set it in our allocated environment
                                        return setSlotIntrinsic(@env_name(), @env_slot(leftname), rhs)

                else if @current_scope.hasReference(leftname)
                        ref = @current_scope.getReference(leftname)
                        if ref.binding.type is 'local' or ref.binding.type is 'arg'
                                declaringScope = ref.binding.declaringScope
                                declaringEnv = declaringScope.env
                                if declaringEnv?.hasSlot(leftname)
                                        return setSlotIntrinsic(declaringEnv.name, declaringEnv.getSlot(leftname), rhs)
                                else
                                        return intrinsic(setLocal_id, [n.left, rhs])
                        else if ref.binding.type is 'global'
                                throw new Error("setGlobal bzzzt")
                        else
                                throw new Error("unhandled binding type #{ref.binding.type}")

                intrinsic(setLocal_id, [n.left, rhs])
                                                
        visitIdentifier: (n) ->
                referents = @current_scope.getReferents(n.name)
                if referents?
                        for referent in referents
                                if referent.referencingScope.location.func isnt @current_scope.location.func
                                        # it's closed over, so we need to set it in our allocated environment
                                        return slotIntrinsic(@env_name(), @env_slot(n.name))
                else if @current_scope.hasReference(n.name)
                        ref = @current_scope.getReference(n.name)
                        if ref.binding.type is 'local' or ref.binding.type is 'arg'
                                declaringScope = ref.binding.declaringScope
                                declaringEnv = declaringScope.env
                                if declaringEnv?.hasSlot(n.name)
                                        return slotIntrinsic(declaringEnv.name, declaringEnv.getSlot(n.name))
                                else
                                        return intrinsic(getLocal_id, [n])
                        else if ref.binding.type is 'global'
                                return intrinsic(getGlobal_id, [n])
                        else
                                throw new Error("unhandled binding type #{ref.binding.type}")

                intrinsic(getLocal_id, [n])

        visitObjectExpression: (n) ->
                for property in n.properties
                        if property.key.type is ComputedPropertyKey
                                property.key = @visit property.key
                        property.value = @visit property.value
                n

        visitCatchClause: (n) ->
                # don't visit the parameter here or else we'll try to rewrite it as %get*(param-name)
                n.body = @visitBlock(n.body)
                n
                
        visitLabeledStatement: (n) ->
                # we need to override this method so we can skip the identifier being used as the label
                n.body  = @visit n.body
                n

class FlattenDeclarations extends TransformPass
        constructor: (options, @filename) ->
                super

        visitBlock: (n) ->
                decl_map = new Map
                n = super(n, decl_map)
                new_body = []
                for s in n.body
                        if decl_map.has(s)
                                new_body = new_body.concat(decl_map.get(s))
                        else
                                new_body.push(s)
                n.body = new_body
                n
                
        visitVariableDeclaration: (n, decl_map) ->
                return super(n,decl_map) if n.declarations.length == 1

                decl_replacement = (b.variableDeclaration(n.kind, decl.id, if decl.init then @visit(decl.init) else b.undefined()) for decl in n.declarations)
                
                decl_map.set(n, decl_replacement)
                n
        
class CollectScopeNestingInfo extends TransformPass
        constructor: (options, @filename) ->
                super
                @options = options
                @block_stack = new Stack
                @func_stack = new Stack
                @current_scope = null
                @scopes = []
                
        visitVariableDeclarator: (n) ->
                # skip the id
                n.init = @visit n.init
                n

        visit_stmt_array: (stmts, initial_bindings) ->
                return this_scope
                
        visitBlock: (n, initial_bindings) ->
                this_scope = new Scope(new Location(n, @func_stack.top))
                @scopes.push(this_scope)
                
                if initial_bindings?
                        this_scope.addBinding(binding) for binding in initial_bindings
                # we have to gather decls before visiting our children
                # so that if they refer to ids in this scope, we can
                # create the proper Reference objects
                for s in n.body
                        if s.type is VariableDeclaration
                                for d in s.declarations
                                        this_scope.addBinding(new LocalBinding(d.id.name, s.kind is 'const'))
                        else if s.type is FunctionDeclaration and s.id?
                                this_scope.addBinding(new LocalBinding(s.id.name, false, this_scope))

                @block_stack.push(n)
                this_scope.parentScope = @current_scope
                @current_scope = this_scope
                super(n)
                @current_scope = this_scope.parentScope
                @block_stack.pop()

                Object.defineProperty(n, 'scope', { value: this_scope })
                n        

        visitFunction: (n) ->
                allFunctions.push(n)
                #param_bindings = (new Binding(p.name, 'arg', false) for p in n.params)
                @func_stack.push(n)
                n.body = @visitBlock(n.body) #, param_bindings)
                @func_stack.pop()
                n

        visitCatchClause: (n) ->
                # visit our body with a new local binding for the catch parameter
                n.body = @visitBlock(n.body, [new LocalBinding(n.param.name, false)])
                n

        find_binding_in_scope: (ident) ->
                name = ident.name
                s = @current_scope
                while s?
                        return new Reference(s.getBinding(name)) if s.hasBinding(name)
                        s = s.parentScope

                if hasOwnProperty.call runtime_globals, name
                        return new Reference(new GlobalBinding(name))

                if @options.warn_on_undeclared
                        reportWarning("undeclared identifier `#{ident.name}'", @filename, ident.loc)
                        binding = global_bindings.get(ident.name)
                        if not binding?
                                binding = new GlobalBinding(ident.name, false)
                                global_bindings.set(ident.name, binding)
                        return new Reference(binding)
                else
                        reportError(ReferenceError, "undeclared identifier `#{ident.name}'", @filename, ident.loc)
                null

        visitIdentifier: (n) ->
                @current_scope.addReference(@find_binding_in_scope(n))
                n

        visitObjectExpression: (n) ->
                for property in n.properties
                        if property.key.type is ComputedPropertyKey
                                property.key = @visit property.key
                        property.value = @visit property.value
                n

        visitCallExpression: (n) ->
                # if it's one of our get/set Slot/Local/Global intrinsics, bail
                return n if is_getset_intrinsic(n)
                
                # otherwise we need to visit the args
                if is_intrinsic(n)
                        n.arguments = @visit n.arguments
                        return n
                super
                
        visitLabeledStatement: (n) ->
                # we need to override this method so we can skip the identifier being used as the label
                n.body  = @visit n.body
                n

placeEnvironments = (scopes) ->
        envs = []
        # we look for scopes that have a non-zero number of referents,
        # and check if they're within the same function.
        # 
        # for referents in the same function, we're guaranteed that
        # they exist down the scope stack for us, and we don't need an
        # environment.
        # 
        # for referents in a different function, we need to add an
        # environment

        func_to_env = new Map

        # this loop figures out based on our lexical scopes the
        # environment we need to pass to every function when calling
        # %makeClosure.
        for s in scopes
                #console.log "analyzing scope #{s.scope_id}, belonging to function #{s.location.func.id.name}"

                env = new Environment("%env_#{envs.length}", s)

                s.referents.forEach (reflist) ->
                        reflist.forEach (ref) ->
                                referencingScope = ref.referencingScope
                                if referencingScope.location.func isnt s.location.func
                                        env.addSlot(ref.binding.name)

                                        func_env = func_to_env.get(referencingScope.location.func)
                                        #console.log("reference into scope #{s.scope_id} from function #{referencingScope.location.func.id.name}")
                                        if not func_env
                                                func_env = env
                                                func_to_env.set(referencingScope.location.func, func_env)
                                        else
                                                if func_env.scope.isAncestorOf(referencingScope) #referencingScope.isAncestorOf(func_env.scope) 
                                                        #console.log "referencingScope is an ancestor of #{env.name}"
                                                        env.addSlot("%parent")
                                                        env.parent_env_name = func_env.name
                                                        func_env = env
                                                        func_to_env.set(referencingScope.location.func, func_env)

                if env.slot_map.size() > 0
                        #console.log "we got a live one here, scope #{s.scope_id} needs an environment, so it got #{env.name}"
                        #console.log "it's closed over references are #{JSON.stringify(env.slot_map.keys())}"
                        
                        s.env = env
                        envs.push env

        # this loop adds the environment parameter to the functions
        for func in allFunctions
                func_body = func.body
                func_stmt = func_body.body
                
                if (func_to_env.has(func))
                        env = func_to_env.get(func)
                        env_name = env.name

                        if env.slot_map.has("%parent")
                                func_stmt.unshift(b.letDeclaration(b.identifier(env.parent_env_name), slotIntrinsic(env.name, env.getSlot("%parent"))))

                else
                        # find an enclosing scope with an environment.  if we can't find one, use %env_unused
                        s = func_body.scope.parentScope
                        while s?
                                break if s.env?
                                s = s.parentScope

                        env_name = if s? then s.env.name else "%env_unused"

                func.params.unshift(b.identifier(env_name))

        for s in scopes
                if s.env
                        body = s.location.block.body
                        env = s.env
                        
                        if env.slot_map.has("%parent")
                                body.unshift(b.expressionStatement(b.assignmentExpression(slotIntrinsic(env.name, env.getSlot("%parent")), '=', b.identifier(env.parent_env_name))))

                        body.unshift(b.letDeclaration(b.identifier(env.name), intrinsic(makeClosureEnv_id, [b.literal(env.slot_map.size())])))


exports.Convert = (options, filename, tree) ->
        flattenDecls = new FlattenDeclarations(options, filename)
        collectScopes = new CollectScopeNestingInfo(options, filename)
        substituteVariables = new SubstituteVariables(options, filename)

        tree = flattenDecls.visit(tree)

        tree = collectScopes.visit(tree)
        
        placeEnvironments(collectScopes.scopes)

        tree = substituteVariables.visit(tree)

        tree
