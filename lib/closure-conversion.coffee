esprima = require 'esprima'
syntax = esprima.Syntax

{ Set } = require 'set'
{ NodeVisitor } = require 'nodevisitor'
{ deep_copy_object, map, foldl } = require 'echo-util'

decl_names = (arr) ->
        result = []
        for n in arr
                if n.declarations?
                        result = result.concat (decl.id.name for decl in n.declarations)
                else if n.id?
                        result.push n.id.name
        new Set result

param_names = (arr) ->
        new Set map ((id) -> id.name), arr

collect_decls = (body) ->
       statement for statement in body when statement.type is syntax.VariableDeclaration or statement.type is syntax.FunctionDeclaration

# TODO: convert this to a visitor
free = (exp) ->
        if not exp?
                return new Set
        switch exp.type
                when syntax.Program
                        decls = decl_names collect_decls exp.body
                        uses = Set.union.apply null, (map free, exp.body)
                        exp.ejs_free_vars = uses.subtract decls
                when syntax.FunctionDeclaration
                        exp.ejs_free_vars = (free exp.body).subtract (param_names exp.params)
                        exp.ejs_decls = exp.body.ejs_decls.union (param_names exp.params)
                when syntax.FunctionExpression
                        exp.ejs_free_vars = (free exp.body).subtract (param_names exp.params)
                        exp.ejs_decls = exp.body.ejs_decls.union (param_names exp.params)
                when syntax.BlockStatement
                        decls = decl_names collect_decls exp.body
                        uses = Set.union.apply null, map free, exp.body
                        uses.subtract decls
                        exp.ejs_decls = decls
                when syntax.CatchClause         then (free exp.body).subtract (new Set [exp.param.name])
                when syntax.VariableDeclaration then Set.union.apply null, (map free, exp.declarations)
                when syntax.VariableDeclarator  then free exp.init
                when syntax.ExpressionStatement then free exp.expression
                when syntax.Identifier          then new Set [exp.name]
                when syntax.ReturnStatement     then free exp.argument
                when syntax.BinaryExpression    then (free exp.left).union free exp.right
                when syntax.LogicalExpression   then (free exp.left).union free exp.right
                when syntax.MemberExpression    then free exp.object # we don't traverse into the property
                when syntax.CallExpression      then Set.union.apply null, [(free exp.callee)].concat (map free, exp.arguments)
                when syntax.Literal             then new Set
                when syntax.IfStatement         then Set.union.apply null, [(free exp.test), (free exp.cconsequent), free (exp.alternate)]
                when syntax.AssignmentExpression then (free exp.left).union free exp.right
                
                else throw "shouldn't reach here, type of node = #{exp.type}"
                #else new Set

#
# each element in env is an object of the form:
#   { exp: ...     // the AST node corresponding to this environment.  will be a function or a BlockStatement. (right now just functions)
#     id:  ...     // the number/id of this environment
#     decls: ..    // a set of the variable names that are declared in this environment
#     closed: ...  // a set of the variable names that are used from this environment (i.e. the ones that need to move to the environment)
#     parent: ...  // the parent environment object
#   }
# 
LocateEnvVisitor = class LocateEnvVisitor extends NodeVisitor
        constructor: ->
                super
                @envs = []
                @env_id = 0
                
        visitFunction: (n) ->
                current_env = @envs[0]
                new_env = { id: @env_id, decls: (if n.ejs_decls? then n.ejs_decls else new Set), closed: new Set, parent: current_env }
                @env_id += 1
                n.ejs_env = new_env
                @envs.unshift new_env
                super
                @envs = @envs.slice 1
                n

        visitVariableDeclarator: (n) ->
                # we override this because we don't want to visit the id, just the initializer
                n.init = @visit n.init
                n

        visitIdentifier: (n) ->
                # find the environment in the env-stack that includes this variable's decl.  add it to the env's .closed set.
                current_env = @envs[0]
                env = current_env
                while env?
                        if env.decls.member n.name
                                if env.closed.member n.name
                                        n.ejs_substitute = true
                                else if env isnt current_env
                                        env.closed.add n.name
                                        n.ejs_substitute = true
                                return n
                        else
                                env = env.parent
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
class LambdaLift extends NodeVisitor
        constructor: (module) ->
                super
                @gen = 0
                @functions = []
                @mappings = []

        genGlobalFunctionName: (x) ->
                name =  "__ejs_function_#{x}#{@gen}"
                @gen += 1
                name

        genAnonymousFunctionName: ->
                name =  "__ejs_anonymous_#{@gen}"
                @gen += 1
                name
        
        currentMapping: -> if @mappings.length > 0 then @mappings[0] else {}

        visitProgram: (program) ->
                super
                program.body = @functions.concat program.body
                program
                
        visitFunction: (n) ->
                if n.type is syntax.FunctionDeclaration
                        n.body = @visit n.body
                        n
                else
                        if n.id?.name?
                                global_name = @genGlobalFunctionName n.id.name
                                @currentMapping()[n.id.name] = global_name
                        else
                                global_name = @genAnonymousFunctionName()

                        n.type = syntax.FunctionDeclaration
                        n.id =
                                type: syntax.Identifier
                                name: global_name

                        @functions.push n

                        new_mapping = deep_copy_object @currentMapping()

                        @mappings.unshift new_mapping
                        n.body = @visit n.body
                        @mappings = @mappings.slice(1)

                        return {
                                type: syntax.Identifier,
                                name: global_name
                        }

create_identifier = (x) ->
        type: syntax.Identifier,
        name: x

# 1) allocates the environment at the start of the n
# 2) adds mappings for all .closed variables
class SubstituteVariables extends NodeVisitor
        constructor: (module) ->
                super
                @mappings = []

        currentMapping: -> if @mappings.length > 0 then @mappings[0] else {}
        
        visitIdentifier: (n) ->
                if n.ejs_substitute?
                        a = @currentMapping()[n.name]
                        if not a?
                                console.warn "missing mapping for #{n.name}"
                                throw "InternalError 2"

                        return a
                n

        visitVariableDeclaration: (n) ->
                rv = []
                for decl in n.declarations
                        decl.init = @visit decl.init
                        a = @currentMapping()[decl.id.name]
                        if a?
                                rv.push {
                                        type: syntax.ExpressionStatement,
                                        expression:
                                                type: syntax.AssignmentExpression,
                                                operator: "="
                                                left:
                                                        type: syntax.MemberExpression,
                                                        computed: false,
                                                        object: @currentMapping()["%env"]
                                                        property: create_identifier decl.id.name
                                                right: decl.init
                                }
                        else
                                rv.push {
                                        type: syntax.VariableDeclaration
                                        declarations: [decl]
                                        kind: n.kind
                                }
                rv                        
                
                
        visitFunction: (n) ->
                if n.ejs_env.closed.empty()
                        n.body.body.unshift
                                type: syntax.VariableDeclaration,
                                declarations: [{
                                        type: syntax.VariableDeclarator
                                        id:  create_identifier "%env_#{n.ejs_env.id}"
                                        init:
                                                type: syntax.Literal
                                                value: null
                                }],
                                kind: "var"
                        super
                else
                        # okay, we know we need a fresh environment in this function
                        
                        # insert environment creation (at the start of the function body)
                        n.body.body.unshift
                                type: syntax.VariableDeclaration,
                                declarations: [{
                                        type: syntax.VariableDeclarator
                                        id:  create_identifier "%env_#{n.ejs_env.id}"
                                        init:
                                                type: syntax.ObjectExpression
                                                properties: []
                                }],
                                kind: "var"

                        # we need to push assignments of any closed over parameters into the environment at this point
                        for param in n.params
                                if n.ejs_env.closed.member param.name
                                        n.body.body.splice 1, 0, {
                                                type: syntax.ExpressionStatement
                                                expression:
                                                        type: syntax.AssignmentExpression,
                                                        operator: "="
                                                        left:
                                                                type: syntax.MemberExpression,
                                                                computed: false,
                                                                object: create_identifier "%env_#{n.ejs_env.id}"
                                                                property: create_identifier param.name
                                                        right: create_identifier param.name
                                        }

                        # XXX if there are existing mappings prepend "%env." (a MemberExpression) to them

                        # add mappings for all variables in .closed from "x" to "%env.x"

                        new_mapping = deep_copy_object @currentMapping()
                        new_mapping["%env"] = create_identifier "%env_#{n.ejs_env.id}"
                        n.ejs_env.closed.map (sym) ->
                                new_mapping[sym] =
                                        type: syntax.MemberExpression,
                                        computed: false,
                                        object: create_identifier "%env"
                                        property: create_identifier sym

                        @mappings.unshift new_mapping
                        super
                        @mappings = @mappings.slice(1)

                # we need to replace function declarations of the form:
                #   function X () { ...body... }
                # 
                # with:
                #   var X = makeClosure(%current_env, function () { ...body... });
                #
                #
                # function expressions are easier:
                # 
                #    function X () { ...body... }
                #
                # replace inline with:
                #
                #    makeClosure(%current_env, function () { ...body... })

                if n.ejs_env.parent
                        if n.type is syntax.FunctionDeclaration
                                call_exp =
                                        type: syntax.CallExpression,
                                        callee: create_identifier "%makeClosure"
                                        arguments: [ (create_identifier "%env_#{n.ejs_env.parent.id}"), n ]
                                var_decl =
                                        type: syntax.VariableDeclarator
                                        id: n.id
                                        init: call_exp

                                # this is kinda lame... not sure if it's necessary.  at the very least we need to keep track of the function's
                                #  name so we can use it when generating the native function.
                                # n.id = null
                                n.type = syntax.FunctionExpression
                                
                                return {
                                        type: syntax.VariableDeclaration,
                                        declarations: [var_decl]
                                        kind: "var"
                                }
                        else # n.type is syntax.FunctionExpression
                                call_exp =
                                        type: syntax.CallExpression,
                                        callee: create_identifier "%makeClosure"
                                        arguments: [ (create_identifier "%env_#{n.ejs_env.parent.id}"), n ]
                                return call_exp
                n        
                
        visitCallExpression: (n) ->
                super

                # we need to replace calls of the form:
                #   X (arg1, arg2, ...)
                # 
                # with
                #   invokeClosure(X, %argCount, arg1, arg2, ...);

                arg_count = n.arguments.length

                n.arguments.unshift { type: syntax.Literal, value: arg_count }
                n.arguments.unshift n.callee
                n.callee = create_identifier "%invokeClosure"
                n



exports.convert = (tree) ->
        #console.warn "before:"
        #console.warn escodegen.generate tree

        # first we decorate the tree with free variable usage
        free tree

        # traverse the tree accumulating info about which function level defined a given variable.
        locate_envs = new LocateEnvVisitor
        tree2 = locate_envs.visit tree

        #console.warn "after LocateEnvVisitor:"
        #console.warn escodegen.generate tree2

        substitute_vars = new SubstituteVariables tree2
        tree3 = substitute_vars.visit tree2

        #console.warn "after SubstituteVariables:"
        #console.warn escodegen.generate tree3

        lambda_lift = new LambdaLift tree3
        tree4 = lambda_lift.visit tree3

        #console.warn "after LambdaLift:"
        #console.warn escodegen.generate tree4

        tree4
        