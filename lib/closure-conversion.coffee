esprima = require 'esprima'
syntax = esprima.Syntax
escodegen = require 'escodegen'
debug = require 'debug'

{ Stack } = require 'stack'
{ Set } = require 'set'
{ TreeTransformer } = require 'nodevisitor'

echo_util = require 'echo-util'
genGlobalFunctionName = echo_util.genGlobalFunctionName
genAnonymousFunctionName = echo_util.genAnonymousFunctionName
deep_copy_object = echo_util.deep_copy_object
map = echo_util.map
foldl = echo_util.foldl
reject = echo_util.reject
create_intrinsic = echo_util.create_intrinsic
is_intrinsic = echo_util.is_intrinsic
create_identifier = echo_util.create_identifier
create_string_literal = echo_util.create_string_literal
create_number_literal = echo_util.create_number_literal
startGenerator = echo_util.startGenerator

hasOwnProperty = Object.prototype.hasOwnProperty

#
# each element in env is an object of the form:
#   { exp: ...     // the AST node corresponding to this environment.  will be a function or a BlockStatement. (right now just functions)
#     id:  ...     // the number/id of this environment
#     decls: ..    // a set of the variable names that are declared in this environment
#     closed: ...  // a set of the variable names that are used from this environment (i.e. the ones that need to move to the environment)
#     parent: ...  // the parent environment object
#   }
# 
LocateEnv = class LocateEnv extends TreeTransformer
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

class HoistFuncDecls extends TreeTransformer
        constructor: ->
                @prepends = []
                
        visitFunction: (n) ->
                @prepends.unshift []
                super
                # we're assuming n.body is a BlockStatement here...
                n.body.body = @prepends.shift().concat n.body.body
                n
        
        visitBlock: (n) ->
                super

                new_body = []
                for child in n.body
                        if child.type is syntax.FunctionDeclaration
                                @prepends[0].push child
                        else
                                new_body.push child
                n.body = new_body
                n

# convert all function declarations to variable assignments
# with named function expressions.
# 
# i.e. from:
#   function foo() { }
# to:
#   var foo = function foo() { }
# 
class FuncDeclsToVars extends TreeTransformer
        constructor: -> super
        
        visitFunctionDeclaration: (n) ->
                if n.toplevel
                        super
                else
                        func_exp = n
                        func_exp.type = syntax.FunctionExpression
                        func_exp.body = @visit func_exp.body
                        return {
                                type: syntax.VariableDeclaration,
                                declarations: [{
                                        type: syntax.VariableDeclarator
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
class HoistVars extends TreeTransformer
        constructor: () ->
                super
                @function_stack = new Stack

        visitFunction: (n) ->
                @function_stack.push { func: n, vars: new Set }
                n = super

                create_empty_declarator = (decl_name) ->
                        type: syntax.VariableDeclarator
                        id: create_identifier decl_name
                        init: null

                if @function_stack.top.vars.size() > 0
                        n.body.body.unshift {
                                type: syntax.VariableDeclaration
                                declarations: create_empty_declarator varname for varname in @function_stack.top.vars.keys()
                                kind: "let"
                        }

                @function_stack.pop()
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
                if n.left.type is syntax.VariableDeclaration
                        @function_stack.top.vars.add n.left.declarations[0].id.name
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
                                                type: syntax.AssignmentExpression
                                                left: create_identifier n.declarations[i].id.name
                                                right: @visit n.declarations[i].init
                                                operator: "="

                                        assignments.push assignment

                        # vars are hoisted to the containing function's toplevel scope
                        @function_stack.top.vars.add decl.id.name for decl in n.declarations

                        if assignments.length is 0
                                return { type: syntax.EmptyStatement }
                                
                        # now return the new assignments, which will replace the original variable
                        # declaration node.
                        if assignments.length > 1
                                assign_exp =
                                        type: syntax.SequenceExpression
                                        expressions: assignments
                        else
                                assign_exp = assignments[0]


                        if @skipExpressionStatement
                                return assign_exp
                        else
                                return {
                                        type: syntax.ExpressionStatement
                                        expression: assign_exp
                                }
                n

# this class really doesn't behave like a normal TreeTransformer, as it modifies the tree in-place.
# XXX reformulate this as a TreeVisitor subclass.
class ComputeFree extends TreeTransformer
        constructor: ->
                @call_free = @free.bind @
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

        param_names: (arr) ->
                new Set map ((id) -> id.name), arr

        collect_decls: (body) ->
                statement for statement in body when statement.type is syntax.VariableDeclaration or statement.type is syntax.FunctionDeclaration

        free_blocklike: (exp,body) ->
                decls = @decl_names @collect_decls body
                uses = Set.union.apply null, map @call_free, body
                exp.ejs_decls = decls
                exp.ejs_free_vars = uses.subtract decls
                exp.ejs_free_vars

        # TODO: move this into the @visit method
        free: (exp) ->
                return new Set if not exp?
                        
                switch exp.type
                        when syntax.Program
                                decls = @decl_names @collect_decls exp.body
                                uses = Set.union.apply null, (map @call_free, exp.body)
                                exp.ejs_decls = decls
                                exp.ejs_free_vars = uses.subtract decls
                        when syntax.FunctionDeclaration
                                # this should only happen for the toplevel function we create to wrap the source file
                                param_names = @param_names exp.params
                                param_names.add exp.rest.name if exp.rest?
                                exp.ejs_free_vars = (@free exp.body).subtract param_names
                                exp.ejs_decls = exp.body.ejs_decls.union param_names
                        when syntax.FunctionExpression
                                param_names = @param_names exp.params
                                param_names.add exp.rest.name if exp.rest?
                                exp.ejs_free_vars = (@free exp.body).subtract param_names
                                exp.ejs_decls = param_names.union exp.body.ejs_decls
                        when syntax.ArrowFunctionExpression
                                param_names = @param_names exp.params
                                param_names.add exp.rest.name if exp.rest?
                                exp.ejs_free_vars = (@free exp.body).subtract param_names
                                exp.ejs_decls = param_names.union exp.body.ejs_decls
                        when syntax.LabeledStatement      then exp.ejs_free_vars = @free exp.body
                        when syntax.BlockStatement        then exp.ejs_free_vars = @free_blocklike exp, exp.body
                        when syntax.TryStatement          then exp.ejs_free_vars = Set.union.apply null, [(@free exp.block)].concat (map @call_free, exp.handlers)
                        when syntax.CatchClause
                                param_set = if exp.param?.name? then new Set [exp.param.name] else new Set
                                exp.ejs_free_vars = (@free exp.body).subtract param_set
                                exp.ejs_decls = exp.body.ejs_decls.union param_set
                        when syntax.VariableDeclaration   then exp.ejs_free_vars = Set.union.apply null, (map @call_free, exp.declarations)
                        when syntax.VariableDeclarator    then exp.ejs_free_vars = @free exp.init
                        when syntax.ExpressionStatement   then exp.ejs_free_vars = @free exp.expression
                        when syntax.Identifier            then exp.ejs_free_vars = new Set [exp.name]
                        when syntax.ThrowStatement        then exp.ejs_free_vars = @free exp.argument
                        when syntax.ForStatement          then exp.ejs_free_vars = Set.union.apply null, (map @call_free, [exp.init, exp.test, exp.update, exp.body])
                        when syntax.ForInStatement        then exp.ejs_free_vars = Set.union.apply null, (map @call_free, [exp.left, exp.right, exp.body])
                        when syntax.WhileStatement        then exp.ejs_free_vars = Set.union.apply null, (map @call_free, [exp.test, exp.body])
                        when syntax.DoWhileStatement      then exp.ejs_free_vars = Set.union.apply null, (map @call_free, [exp.test, exp.body])
                        when syntax.SwitchStatement       then exp.ejs_free_vars = Set.union.apply null, [@free exp.discriminant].concat (map @call_free, exp.cases)
                        when syntax.SwitchCase            then exp.ejs_free_vars = @free_blocklike exp, exp.consequent
                        when syntax.EmptyStatement        then exp.ejs_free_vars = new Set
                        when syntax.BreakStatement        then exp.ejs_free_vars = new Set
                        when syntax.ContinueStatement     then exp.ejs_free_vars = new Set
                        when syntax.UpdateExpression      then exp.ejs_free_vars = @free exp.argument
                        when syntax.ReturnStatement       then exp.ejs_free_vars = @free exp.argument
                        when syntax.UnaryExpression       then exp.ejs_free_vars = @free exp.argument
                        when syntax.BinaryExpression      then exp.ejs_free_vars = (@free exp.left).union @free exp.right
                        when syntax.LogicalExpression     then exp.ejs_free_vars = (@free exp.left).union @free exp.right
                        when syntax.MemberExpression      then exp.ejs_free_vars = @free exp.object # we don't traverse into the property
                        when syntax.CallExpression        then exp.ejs_free_vars = Set.union.apply null, [(@free exp.callee)].concat (map @call_free, exp.arguments)
                        when syntax.NewExpression         then exp.ejs_free_vars = Set.union.apply null, [(@free exp.callee)].concat (map @call_free, exp.arguments)
                        when syntax.SequenceExpression    then exp.ejs_free_vars = Set.union.apply null, map @call_free, exp.expressions
                        when syntax.ConditionalExpression then exp.ejs_free_vars = Set.union.apply null, [(@free exp.test), (@free exp.cconsequent), (@free exp.alternate)]
                        when syntax.Literal               then exp.ejs_free_vars = new Set
                        when syntax.ThisExpression        then exp.ejs_free_vars = new Set
                        when syntax.Property              then exp.ejs_free_vars = @free exp.value # we skip the key
                        when syntax.ObjectExpression
                                exp.ejs_free_vars = if exp.properties.length is 0 then (new Set) else Set.union.apply null, map @call_free, (p.value for p in exp.properties)
                        when syntax.ArrayExpression
                                exp.ejs_free_vars = if exp.elements.length is 0 then (new Set) else Set.union.apply null, map @call_free, exp.elements
                        when syntax.IfStatement           then exp.ejs_free_vars = Set.union.apply null, [(@free exp.test), (@free exp.consequent), (@free exp.alternate)]
                        when syntax.AssignmentExpression  then exp.ejs_free_vars = (@free exp.left).union @free exp.right
                
                        else throw "Internal error: unhandled node type '#{exp.type}' in free()"
                exp.ejs_free_vars

                
                
# 1) allocates the environment at the start of the n
# 2) adds mappings for all .closed variables
class SubstituteVariables extends TreeTransformer
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
                                type: syntax.BlockStatement
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
                        n.left = create_identifier left[0].declarations[0].id.name
                        return {
                                type: syntax.BlockStatement
                                body: left.concat [n]
                        }
                else
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
                                                rv.push {
                                                        type: syntax.VariableDeclaration
                                                        declarations: new_declarations
                                                        kind: n.kind
                                                }

                                        # splice in this assignment
                                        rv.push {
                                                type: syntax.ExpressionStatement
                                                expression: 
                                                        type: syntax.AssignmentExpression
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
                                type: syntax.VariableDeclaration
                                declarations: new_declarations
                                kind: n.kind
                        }

                if rv.length is 0
                        rv = { type: syntax.EmptyStatement }
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
                
                new_mapping = deep_copy_object @currentMapping()
                if n.ejs_env.closed.empty() and not n.ejs_env.nested_requires_env
                        n.body.body.unshift
                                type: syntax.VariableDeclaration,
                                declarations: [{
                                        type: syntax.VariableDeclarator
                                        id:  create_identifier this_env_name
                                        init:
                                                type: syntax.Literal
                                                value: null
                                }],
                                kind: "let"
                else
                        # okay, we know we need a fresh environment in this function
                        prepends = []
                        # insert environment creation (at the start of the function body)
                        prepends.push {
                                type: syntax.VariableDeclaration,
                                declarations: [{
                                        type: syntax.VariableDeclarator
                                        id:  create_identifier this_env_name
                                        init: create_intrinsic "%makeClosureEnv", [create_number_literal n.ejs_env.closed.size() + if n.ejs_env.parent? then 1 else 0]
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
                                prepends.push {
                                        type: syntax.ExpressionStatement
                                        expression: create_intrinsic "%setSlot", [ (create_identifier this_env_name), (create_number_literal parent_env_slot), (create_string_literal parent_env_name), (create_identifier parent_env_name) ]
                                }
                                

                        # we need to push assignments of any closed over parameters into the environment at this point
                        for param in n.params
                                if n.ejs_env.closed.has param.name
                                        prepends.push {
                                                type: syntax.ExpressionStatement
                                                expression: create_intrinsic "%setSlot", [ (create_identifier this_env_name), (create_number_literal n.ejs_env.slot_mapping[param.name]), (create_string_literal param.name), (create_identifier param.name) ]
                                        }

                        new_mapping["%slot_mapping"] = n.ejs_env.slot_mapping
                        ###
                        for mapped_id of n.ejs_env.slot_mapping
                                if new_mapping[mapped_id] is undefined
                                        console.log "new_mapping[#{mapped_id}] == undefined"
                        ###

                        flatten_memberexp = (exp, mapping) ->
                                if exp.type isnt syntax.CallExpression
                                        [create_number_literal mapping[exp.name]]
                                else
                                        (flatten_memberexp exp.arguments[0], mapping).concat [exp.arguments[1]]

                        prepend_environment = (exps) ->
                                obj = create_identifier this_env_name
                                for prop in exps
                                        obj = create_intrinsic "%slot", [ obj, prop ]
                                obj

                        # if there are existing mappings prepend "%env." (a MemberExpression) to them
                        for mapped of new_mapping
                                val = new_mapping[mapped]
                                if mapped isnt "%slot_mapping"
                                        new_mapping[mapped] = prepend_environment (flatten_memberexp val, n.ejs_env.slot_mapping)
                        
                        # and add mappings for all variables in .closed from "x" to "%env.x"

                        new_mapping["%env"] = create_identifier this_env_name
                        n.ejs_env.closed.map (sym) ->
                                new_mapping[sym] = create_intrinsic "%slot", [ (create_identifier this_env_name), (create_number_literal n.ejs_env.slot_mapping[sym]), (create_string_literal sym) ]

                # remove all mappings for variables declared in this function
                if n.ejs_decls?
                        new_mapping = reject new_mapping, (sym) -> (n.ejs_decls.has sym) and not (n.ejs_env.closed.has sym)

                @mappings.push new_mapping
                @visitFunctionBody n
                if prepends?
                        n.body.body = prepends.concat n.body.body
                @mappings.pop()

                # convert function expressions to an explicit closure creation, so:
                # 
                #    function X () { ...body... }
                #
                # replace inline with:
                #
                #    makeClosure(%current_env, "X", function X () { ...body... })

                if not n.toplevel
                        if n.type is syntax.FunctionDeclaration
                                throw "there should be no FunctionDeclarations at this point"
                        else # n.type is syntax.FunctionExpression
                                return if n.id?
                                        create_intrinsic "%makeClosure", [ (if n.ejs_env.parent? then (create_identifier parent_env_name) else { type: syntax.Literal, value: null}), (create_string_literal n.id.name), n ]
                                else
                                        create_intrinsic "%makeAnonClosure", [ (if n.ejs_env.parent? then (create_identifier parent_env_name) else { type: syntax.Literal, value: null}), n ]
                n
            catch e
                console.warn "exception: " + e
                console.warn "compiling the following code:"
                console.warn escodegen.generate n
                throw e
                
        visitCallExpression: (n) ->
                super

                # replace calls of the form:
                #   X (arg1, arg2, ...)
                # 
                # with
                #   invokeClosure(X, %this, %argCount, arg1, arg2, ...);

                @function_stack.top.scratch_size = Math.max @function_stack.top.scratch_size, n.arguments.length

                create_intrinsic "%invokeClosure", [n.callee].concat n.arguments

        visitNewExpression: (n) ->
                super

                # replace calls of the form:
                #   new X (arg1, arg2, ...)
                # 
                # with
                #   invokeClosure(X, %this, %argCount, arg1, arg2, ...);

                @function_stack.top.scratch_size = Math.max @function_stack.top.scratch_size, n.arguments.length

                rv = create_intrinsic "%invokeClosure", [n.callee].concat n.arguments
                rv.type = syntax.NewExpression
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
class LambdaLift extends TreeTransformer
        constructor: (@filename) ->
                @functions = []
                @mappings = new Stack
                super

        currentMapping: -> if @mappings.depth > 0 then @mappings.top else Object.create null

        visitProgram: (program) ->
                super
                program.body = @functions.concat program.body
                program

        maybePrependScratchArea: (n) ->
                if n.scratch_size > 0
                        alloc_scratch =
                                type: syntax.ExpressionStatement
                                expression: create_intrinsic "%createArgScratchArea", [ { type: syntax.Literal, value: n.scratch_size } ]
                        n.body.body.unshift alloc_scratch
                
        visitFunctionDeclaration: (n) ->
                n.body = @visit n.body
                @maybePrependScratchArea n
                n

        visitArrowFunctionExpression: (n) ->
                # this doesn't really belong here, but if we're
                # dealing with an arrow function with an expression
                # for a body, replace the body with a block statement
                # with a return statement returning that expression
                if n.expression
                        n.body = {
                                type: syntax.BlockStatement
                                body: [{
                                        type: syntax.ReturnStatement
                                        argument: n.body
                                }]
                        }
                        n.espression = false
                @visitFunctionExpression n

        visitFunctionExpression: (n) ->
                if n.displayName?
                        global_name = genGlobalFunctionName n.displayName, @filename
                else if n.id?.name?
                        global_name = genGlobalFunctionName n.id.name, @filename
                else
                        global_name = genAnonymousFunctionName(@filename)
                
                if n.id?.name?
                        @currentMapping()[n.id.name] = global_name

                n.type = syntax.FunctionDeclaration
                n.id =
                        type: syntax.Identifier
                        name: global_name

                @functions.push n

                new_mapping = deep_copy_object @currentMapping()

                @mappings.push new_mapping
                n.body = @visit n.body
                @mappings.pop()

                @maybePrependScratchArea n

                n.params.unshift create_identifier "%env_#{n.ejs_env.parent.id}"
                
                return {
                        type: syntax.Identifier,
                        name: global_name
                }


class NameAnonymousFunctions extends TreeTransformer
        constructor: -> super
        visitAssignmentExpression: (n) ->
                n = super n
                lhs = n.left
                rhs = n.right

                # if we have the form
                #   <identifier> = function () { }
                # convert to:
                #   <identifier> = function <identifier> () { }
                #if lhs.type is syntax.Identifier and rhs.type is syntax.FunctionExpression and not rhs.id?.name
                #        rhs.display = <something pretty about the lhs>
                #
                if rhs.type is syntax.FunctionExpression and not rhs.id?.name
                        rhs.displayName = escodegen.generate lhs
                n

class MarkLocalAndGlobalVariables extends TreeTransformer
        constructor: ->
                # initialize the scope stack with the global (empty) scope
                @scope_stack = new Stack Object.create null

        findIdentifierInScope: (ident) ->
                for scope in @scope_stack.stack
                        if hasOwnProperty.call scope, ident.name
                                return true
                return false

        intrinsicForIdentifier: (id, prefix) ->
                is_local = @findIdentifierInScope id
                "#{prefix}#{if is_local then 'Local' else 'Global'}"
                
        visitWithScope: (scope, children) ->
                @scope_stack.push scope
                if Array.isArray children
                        rv = (@visit child for child in children)
                else
                        rv = @visit children
                @scope_stack.pop()
                rv

        visitVariableDeclarator: (n) ->
                @scope_stack.top[n.id.name] = true
                n

        visitObjectExpression: (n) ->
                for property in n.properties
                        property.value = @visit property.value
                n

        visitAssignmentExpression: (n) ->
                lhs = n.left
                rhs = @visit n.right
                if n.operator.length is 1
                        new_rhs = rhs
                else
                        new_rhs = {
                                type:     syntax.BinaryExpression,
                                operator: n.operator[0]
                                left:     lhs
                                right:    rhs
                        }
                
                if is_intrinsic "%slot", lhs
                        create_intrinsic "%setSlot", [lhs.arguments[0], lhs.arguments[1], new_rhs]
                else if lhs.type is syntax.Identifier
                        if @findIdentifierInScope lhs
                                create_intrinsic "%setLocal", [lhs, new_rhs]
                        else
                                create_intrinsic "%setGlobal", [lhs, new_rhs]
                else
                        n.left = @visit n.left
                        n.right = new_rhs
                        n
                        

        ###
        visitUpdateExpression: (n) ->
                argument = n.argument
                new_rhs = {
                        type:     syntax.BinaryExpression,
                        operator: n.operator[0]
                        left:     @visit argument
                        right:    { type: syntax.Literal, value: 1, raw: "1" }
                }
                
                if is_intrinsic "%slot", argument
                        create_intrinsic "%setSlot", [argument.arguments[0], argument.arguments[1], new_rhs]
                else # argument.type is syntax.Identifier
                        create_intrinsic (@intrinsicForIdentifier argument, "%set"), [argument, new_rhs]
        ###

        visitBlock: (n) ->
                n.body = @visitWithScope (Object.create null), n.body
                n

        visitCallExpression: (n) ->
                # at this point there shouldn't be call expressions for anything other than intrinsics,
                # so we leave callee alone and just iterate over the args.
                n.arguments = @visit n.arguments
                n

        visitNewExpression: (n) ->
                # at this point there shouldn't be new expressions for anything other than intrinsics,
                # so we leave callee alone and just iterate over the args.
                n.arguments = @visit n.arguments
                n

        visitFunction: (n) ->
                new_scope = Object.create null
                new_scope[p.name] = true for p in n.params
                new_scope[n.rest.name] = true if n.rest?
                new_scope["arguments"] = true
                new_body = @visitWithScope new_scope, n.body
                n.body = new_body
                n

        visitLabeledStatement: (n) ->
                n.body  = @visit n.body
                n

        visitCatchClause: (n) ->
                new_scope = Object.create null
                new_scope[n.param.name] = true
                n.guard = @visit n.guard
                n.body = @visitWithScope new_scope, n.body
                n

        visitIdentifier: (n) ->
                create_intrinsic (@intrinsicForIdentifier n, "%get"), [n]

        visitForIn: (n) ->
                n.right = @visit n.right
                n.body  = @visit n.body
                n

class ReplaceUnaryVoid extends TreeTransformer
        constructor: -> super
        
        visitUnaryExpression: (n) ->
                if n.operator is "void" and n.argument.type is syntax.Literal and n.argument.value is 0
                        return create_intrinsic "%builtinUndefined", []
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
class IIFEIdioms extends TreeTransformer

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

                replacement = { type: syntax.BlockStatement, body: [] }

                replacement.body.push {
                        type: syntax.VariableDeclaration,
                        kind: "let",
                        declarations: [{
                                type: syntax.VariableDeclarator
                                id:   iife_rv_id
                                init: undefined
                        }]
                }

                for i in [0...arity]
                        replacement.body.push {
                                type: syntax.VariableDeclaration,
                                kind: "let",
                                declarations: [{
                                        type: syntax.VariableDeclarator
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
                        n.expression.arguments[2] = create_intrinsic "%getLocal", [iife_rv_id]
                        replacement.body.push n
                else if (is_intrinsic "%setGlobal", n.expression) or (is_intrinsic "setLocal", n.expression)
                        n.expression.arguments[1] = create_intrinsic "%getLocal", [iife_rv_id]
                        replacement.body.push n
                        
                return replacement

        maybeInlineIIFECall: (candidate, n) ->
                return n if candidate.arguments.length isnt 2 or candidate.arguments[1].type isnt syntax.ThisExpression

                iife_rv_id = create_identifier "%iife_rv_#{@iife_generator()}"

                replacement = { type: syntax.BlockStatement, body: [] }

                replacement.body.push {
                        type: syntax.VariableDeclaration,
                        kind: "let",
                        declarations: [{
                                type: syntax.VariableDeclarator
                                id:   iife_rv_id
                                init: undefined
                        }]
                }

                body = candidate.arguments[0].object.arguments[1].body
                body.ejs_iife_rv = iife_rv_id
                body.fromIIFE = true

                replacement.body.push body

                if is_intrinsic "%setSlot", n.expression
                        n.expression.arguments[2] = create_intrinsic "%getLocal", [iife_rv_id]
                        replacement.body.push n
                else if (is_intrinsic "%setGlobal", n.expression) or (is_intrinsic "setLocal", n.expression)
                        n.expression.arguments[1] = create_intrinsic "%getLocal", [iife_rv_id]
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
                else if candidate.arguments[0].type is syntax.MemberExpression and (isMakeClosure candidate.arguments[0].object) and candidate.arguments[0].property.name is "call"
                        return @maybeInlineIIFECall candidate, n
                else
                        return n
                        
                                                                                                
transform_dump_tree = (n, indent=0) ->
        stringified_indent = ('' for i in [0..(indent*2)]).join('| ')
        console.log "#{stringified_indent}.type = syntax.#{n.type}"
        console.log "#{stringified_indent}.js = #{escodegen.generate n, { format: { compact: true } } }"
        console.log "#{stringified_indent}.free = #{n.ejs_free_vars}" if n.ejs_free_vars?
        console.log "#{stringified_indent}.env.decls = #{n.ejs_env.decls}" if n.ejs_env?
        console.log "#{stringified_indent}.env.closed = #{n.ejs_env.closed}" if n.ejs_env?
        props_to_skip = ["type", "id", "loc", "ejs_free_vars", "ejs_decls", "ejs_env"]
        Object.getOwnPropertyNames(n).forEach (propName) ->
                if (props_to_skip.indexOf propname) is -1
                        if n[propname]?.type?
                                console.log "#{stringified_indent}.#{propname}:"
                                transform_dump_tree n[propname], indent+1
                        else if Array.isArray n[propname]
                                console.log "#{stringified_indent}.#{propname}: ["
                                (transform_dump_tree element, indent+1) for element in n[propname]
                                console.log "#{stringified_indent}]"
                        else
                                console.log "#{stringified_indent}.#{propname} = #{JSON.stringify n[propname]}"
        undefined

passes = [
        HoistFuncDecls
        FuncDeclsToVars
        HoistVars
        ComputeFree
        LocateEnv
        NameAnonymousFunctions
        SubstituteVariables
        MarkLocalAndGlobalVariables
        ReplaceUnaryVoid
        IIFEIdioms
        LambdaLift
        ]

exports.convert = (tree, filename) ->
        debug.log "before:"
        debug.log -> escodegen.generate tree

        passes.forEach (passType) ->
                pass = new passType(filename)
                tree = pass.visit tree
                debug.log 2, "after: #{passType.name}"
                debug.log 2, -> escodegen.generate tree

        tree
