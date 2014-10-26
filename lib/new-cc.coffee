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

assignStmt             = (l, op, r) -> b.expressionStatement(b.assignmentExpression(l, op, r))
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
                @children = []
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

        isFunctionBodyScope: () -> @location.block is @location.func.body
        isAncestorOf: (s) ->
                _s = @parentScope
                while _s?
                        return true if _s is s
                        _s = _s.parentScope
                false
                
        differentFunction: (otherscope) -> @location.func != otherscope.location.func

        debugString: () ->
                str = "scope at line #{@location.block.loc?.start.line}"
                if @isFunctionBodyScope()
                        str = "#{str} : for function #{@location.func.id?.name}"
                str = "#{str} : environment = #{@env?.name}"
                str
        

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
        constructor: (@id, scope, @level) ->
                @name = "%env_#{@id}"
                @slot_map = new Map
                @parentEnv = null

        hasSlots: () -> @slot_map.size() > 0
        hasSlot: (name) -> @slot_map.has(name)
        getSlot: (name) ->
                rv = @slot_map.get(name)
                throw new Error("environment #{@name} doesn't contain slot for #{name}") if rv is undefined
                rv
        addSlot: (name) ->
                return if @slot_map.has(name)
                @slot_map.set(name, @slot_map.size())
                return
        slotCount: () -> @slot_map.size()

        addChild: (env) ->
                if env?.parentEnv? and env?.parentEnv isnt @
                        throw new Error("attempting to set #{env.name}'s parent to #{@name}, but it already has a a parent, #{env.parentEnv.name}")
                env.addSlot(@name)
                env.parentEnv = @
                return

        toString: -> @name

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
                                if referent.referencingScope.differentFunction(@current_scope)
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

                intrinsic_args = [if n.params[0].name is "%env_unused" then b.undefined() else b.identifier(n.params[0].name, n.loc)]
                        
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
                                if referent.referencingScope.differentFunction(@current_scope)
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
                                if referent.referencingScope.differentFunction(@current_scope)
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
                @root_scope = null
                
        visitVariableDeclarator: (n) ->
                # skip the id
                n.init = @visit n.init
                n

        doWithScope: (scope, fn) ->
                scope.parentScope = @current_scope
                if @current_scope
                        @current_scope.children.push scope
                @current_scope = scope
                fn()
                @current_scope = scope.parentScope

        doWithBlock: (n, fn) ->
                @block_stack.push(n)
                fn()
                @block_stack.pop()

        doWithFunc: (n, fn) ->
                @func_stack.push(n)
                fn()
                @func_stack.pop()
                
        visitBlock: (n, initial_bindings) ->
                this_scope = new Scope(new Location(n, @func_stack.top))
                if @root_scope is null
                        @root_scope = this_scope

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

                @doWithScope this_scope, =>
                        @doWithBlock n, =>
                                super(n)
                Object.defineProperty(n, 'scope', { value: this_scope })
                n        

        visitFunction: (n) ->
                #debug.log "function #{n.id?.name} has idx of #{allFunctions.length}"
                
                allFunctions.push(n)
                #param_bindings = (new Binding(p.name, 'arg', false) for p in n.params)
                @doWithFunc n, =>
                        n.body = @visitBlock(n.body) #, param_bindings
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

placeEnvironments = (root_scope) ->
        env_id = 0
        
        # A map : function -> [environment]
        func_to_envs = new Map

        get_func_envs = (f) ->
                func_idx = allFunctions.indexOf(f)
                func_envs = func_to_envs.get(func_idx)
                if not func_envs?
                        func_envs = []
                        func_to_envs.set(func_idx, func_envs)
                func_envs

        add_func_env = (func_envs, env) ->
                if func_envs[env.level]?
                        if func_envs[env.level] isnt env
                                throw new Error("multiple paths to an environment?  shouldn't be possible.")
                else
                        func_envs[env.level] = env
                
        dump_func_envs = () ->
                func_to_envs.forEach (v,k) ->
                        debug.log "function #{allFunctions[k].id?.name} (#{k}) requires these environments:"
                        if not v? or v.length is 0
                                debug.log "  none!"
                        else
                                for e in v
                                        if e?
                                                debug.log "  env: #{e.name}"

        #
        # given an array of arrays of the form:
        #
        # [    ,    , e2 ,    ]
        # [ e0 , e1 ,    ,    ]
        #
        # returns
        #
        # [ e0 , e1 , e2 ,    ]
        #
        # used for merging the func_env arrays returned from children
        # 
        flatten_func_envs = (fe_arr) ->
                rv = []
                for fe in fe_arr
                        if fe?
                                idx = 0
                                while idx < fe.length
                                        e = fe[idx]
                                        rv[idx] = e if e? # should probably check if !rv[idx] or rv[idx] === e
                                        idx += 1
                rv
                
        dump_scopes = (s, level) ->
                #debug.log s.debugString()
                s.children.forEach (c) ->
                        #debug.indent()
                        dump_scopes(c, level + 1)
                        #debug.unindent()

        walk_scope1 = (s, level) ->
                #
                # Collect external references to bindings defined in this scope.
                #
                #          referent                 reference
                #     s <-------------> binding <---------------> referencingScope
                #
                #debug.log "dealing with scope from line #{s.location.block.loc?.start.line}, environment will be #{env_id}"
                env = new Environment(env_id, s, level)
                env_id += 1
                
                # walk over this scope's referents.  if any of this scope's
                # bindings are referred to from outside the function, we need
                # an environment
                s.referents.forEach (reflist) ->
                        reflist.forEach (ref) ->
                                referencingScope = ref.referencingScope
                                #debug.log "referent name is #{ref.binding.name}, s.func = #{s.location.func.id?.name}/#{s.location.func.loc?.start.line}, referencing scope = #{referencingScope.location.func.id?.name}/#{referencingScope.location.func.loc?.start.line}"
                                if referencingScope.differentFunction(s)
                                        # the scopes are in different functions
                                        #debug.log "reference to '#{ref.binding.name}' from outside declaring function (in function #{referencingScope.location.func.id?.name})!"

                                        # and add a slot for the referent
                                        env.addSlot(ref.binding.name)

                                        # also, mark the referencing scope's function as needing this environment
                                        func_envs = get_func_envs(referencingScope.location.func)
                                        add_func_env(func_envs, env)


                if env.slotCount() > 0
                        #debug.log "creating environment '#{env.name}' for function #{s.location.func.id?.name}"
                        s.env = env

                # recurse into our child scopes.
                child_func_envs = s.children.map (c) ->
                        #debug.indent()
                        ce = walk_scope1(c, level + 1)
                        #debug.unindent()
                        ce

                child_env_reqs = flatten_func_envs(child_func_envs)
                        
                # we're back in the scope passed to this function,
                # having visited all parents and all children.  we
                # should now know exactly which parent scopes have
                # environments, and should be able to calculate the
                # path to any bindings we reference.

                #debug.log "before removing our environment, function #{s.location.func.id?.name} has the following required (from children) environments: #{env for env in child_env_reqs}"

                if s.env?
                        idx = child_env_reqs.indexOf s.env
                        if idx isnt -1
                                #debug.log "removing env #{s.env.name} from list of required environments"
                                child_env_reqs.splice(idx, 1)
                        
                #debug.log "function #{s.location.func.id?.name} has the following required (from children) environments: #{env for env in child_env_reqs}"

                s.nestedEnvironments = child_env_reqs
                
                if s.isFunctionBodyScope()
                        func = s.location.func
                        func_idx = allFunctions.indexOf(func)
                        #debug.log(" scope is function body scope for #{func_idx} #{func.id?.name}/#{func.loc?.start.line}");

                        func_envs = get_func_envs(func)

                        # add the nested environments required by our descendents (that have not been added somewhere in this function) as though they are required by us
                        s.nestedEnvironments.forEach (nested_env) ->
                                return if not nested_env?
                                #debug.log "adding #{nested_env}"
                                add_func_env(func_envs, nested_env)

                        #debug.log("after adding nested environments, func_envs for function is #{fenv for fenv in func_envs}")
                        
                        return func_envs

                return s.nestedEnvironments
                

        # for every environment, calculate the parent they must have by all the paths we've computed in func_to_envs
        collapse_paths = () ->
                parent_envs = new Map()

                func_to_envs.forEach (func_envs, func) ->
                        return if func_envs.length is 0

                        func_envs = func_envs.filter (a_env) -> a_env?
                                
                        idx = func_envs.length - 1
                        while idx >= 1
                                current_e = func_envs[idx]
                                prospective_parent = func_envs[idx-1]
                                if not parent_envs.has(current_e) or parent_envs.get(current_e).level < prospective_parent.level
                                        parent_envs.set(current_e, prospective_parent)
                                idx -= 1

                parent_envs.forEach (e, p_e) ->
                        e.addChild(p_e)

                # now insert dependencies for parent envs between environments that require them
                func_to_envs.forEach (func_envs, func) ->
                        return if func_envs.length < 2

                        collapsed_func_envs = func_envs.filter (a_env) -> a_env?

                        idx = collapsed_func_envs.length - 1
                        while idx >= 1
                                current_e = collapsed_func_envs[idx]
                                prior_e = collapsed_func_envs[idx-1]
                                if current_e.parentEnv isnt prior_e
                                        # we need to walk current_e's parent chain until we reach prior_e, adding the environments to func_envs
                                        e = current_e.parentEnv
                                        while e isnt prior_e
                                                add_func_env(func_envs, e)
                                                e = e.parentEnv
                                idx -= 1
                        

        walk_scope2 = (s, level) ->
                #debug.log "walk_scope2 for scope #{s.debugString()}"
                
                if s.env?
                        if s.env.parentEnv? and s.env.parentEnv.slotCount() > 0
                                #debug.log "outputting parent environment assignment.  parentEnv.name = #{s.env.parentEnv.name}, parentEnv.slotCount = #{s.env.parentEnv.slotCount()}"
                                s.location.block.body.unshift(b.expressionStatement(setSlotIntrinsic(s.env.name, s.env.getSlot(s.env.parentEnv.name), b.identifier(s.env.parentEnv.name))))
                        s.location.block.body.unshift(b.letDeclaration(b.identifier(s.env.name), intrinsic(makeClosureEnv_id, [b.literal(s.env.slot_map.size())])))
                else
                        #debug.log "scope from line #{s.location.block.loc?.start.line} doesn't have environment"

                if s.isFunctionBodyScope()
                        do ->
                                func = s.location.func
                                func_idx = allFunctions.indexOf(func)
                                #debug.log(" ******* scope is function body scope for #{func_idx} #{func.id?.name} #{func.loc?.start.line}");
                                func_envs = get_func_envs(s.location.func)

                                env_assignments = []

                                if func_envs.length is 0
                                        #debug.log "unused environment, func_envs.length = #{func_envs?.length}"
                                        env_name = "%env_unused"
                                else # func_envs.length >= 1
                                        func_envs = func_envs.filter (a_env) -> a_env?

                                        last_idx = func_envs.length-1
                                        
                                        env_name = func_envs[last_idx].name
                                        
                                        last_idx -= 1
                                        
                                        while last_idx >= 0
                                                #debug.log "adding const #{func_envs[last_idx].name} = slotIntrinsic(#{func_envs[last_idx+1].name}, #{func_envs[last_idx+1].name}.getSlot(#{func_envs[last_idx].name}));"
                                                #debug.log "       const #{func_envs[last_idx].name} = slotIntrinsic(#{func_envs[last_idx+1].name}, #{func_envs[last_idx+1].getSlot(func_envs[last_idx].name)});"
                                                env_decl = b.constDeclaration(b.identifier(func_envs[last_idx].name), slotIntrinsic(func_envs[last_idx+1].name, func_envs[last_idx+1].getSlot(func_envs[last_idx].name)))
                                                env_decl.loc = func.body.loc
                                                env_assignments.push env_decl
                                                last_idx -= 1

                                # add the parameter we need
                                func.params.unshift(b.identifier(env_name, func.loc))
                                
                                # and add the assignments of all the environments this function needs
                                if env_assignments.length > 0
                                        func.body.body = env_assignments.concat(func.body.body)

                s.children.forEach (c) ->
                        #debug.indent()
                        walk_scope2(c, level + 1)
                        #debug.unindent()
        
        walk_scope1(root_scope, 0)
        collapse_paths()
        #dump_scopes(root_scope, 0)
        walk_scope2(root_scope, 0)
        #dump_func_envs()
        
class ValidateEnvironments extends TreeVisitor
        is_undefined_literal = (e) ->
                return true if e.type is Literal and e.value is undefined
                e.type is UnaryExpression and e.operator is "void" and e.argument.value is 0
        
        constructor: (options, @filename) ->
                super
                @options = options

        visitCallExpression: (n) ->
                n = super
                # make sure that the environment we assign to a
                # closure is the same as the first arg to the function
                if is_intrinsic(n, makeClosure_id.name) or is_intrinsic(n, makeAnonClosure_id.name)
                        #debug.log escodegen.generate n.arguments[0]
                        #debug.log is_undefined_literal(n.arguments[0])
                        if not is_undefined_literal(n.arguments[0])
                                closure_env = n.arguments[0].name
                                closure_func = n.arguments[if is_intrinsic(n, makeClosure_id.name) then 2 else 1]
                                func_env = closure_func.params[0].name
                                if closure_env isnt func_env
                                        throw new Error("closure created using environment #{closure_env}, while function takes #{func_env}")

                else if is_intrinsic(n, setSlot_id.name)
                        env_name = n.arguments[0].name
                        env_id = env_name.substring("%env_".length)
                        slot = n.arguments[1].value
                else if is_intrinsic(n, slot_id.name)
                        env_name = n.arguments[0].name
                        env_id = env_name.substring("%env_".length)
                        slot = n.arguments[1].value
                n

exports.Convert = (options, filename, tree) ->
        flattenDecls = new FlattenDeclarations(options, filename)
        collectScopes = new CollectScopeNestingInfo(options, filename)
        substituteVariables = new SubstituteVariables(options, filename)

        tree = flattenDecls.visit(tree)

        tree = collectScopes.visit(tree)

        #debug.log escodegen.generate tree
        placeEnvironments(collectScopes.root_scope)

        tree = substituteVariables.visit(tree)

        validator = new ValidateEnvironments(options, filename)
        tree = validator.visit(tree)

        allFunctions = []
        global_bindings = new Map()
        
        tree
