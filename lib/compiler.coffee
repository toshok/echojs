esprima = require 'esprima'
escodegen = require 'escodegen'
syntax = esprima.Syntax
debug = require 'debug'

debug.setLevel 0

{ Set } = require 'set'
{ NodeVisitor } = require 'nodevisitor'
closure_conversion = require 'closure-conversion'

llvm = require 'llvm'

# special key for parent scope when performing lookups
PARENT_SCOPE_KEY = ":parent:"

stringType = llvm.Type.getInt8Ty().pointerTo
boolType = llvm.Type.getInt8Ty()
voidType = llvm.Type.getVoidTy()
int32Type = llvm.Type.getInt32Ty()

ejsValueType = llvm.StructType.create "EjsValue", [int32Type]

EjsValueType = ejsValueType.pointerTo
EjsClosureEnvType = EjsValueType
EjsFuncType = (llvm.FunctionType.get EjsValueType, [EjsClosureEnvType, llvm.Type.getInt32Ty()]).pointerTo

BUILTIN_ARGS = [
  { llvm_type: EjsClosureEnvType } # %env
  { llvm_type: int32Type }         # %argc
]

class LLVMIRVisitor extends NodeVisitor
        constructor: (@module) ->
                @current_scope = {}

                # build up our runtime method table
                @builtins = {
                        invokeClosure: [
                                module.getOrInsertExternalFunction "_ejs_invoke_closure_0", EjsValueType, EjsValueType, llvm.Type.getInt32Ty()
                                module.getOrInsertExternalFunction "_ejs_invoke_closure_1", EjsValueType, EjsValueType, llvm.Type.getInt32Ty(), EjsValueType
                                module.getOrInsertExternalFunction "_ejs_invoke_closure_2", EjsValueType, EjsValueType, llvm.Type.getInt32Ty(), EjsValueType, EjsValueType
                                module.getOrInsertExternalFunction "_ejs_invoke_closure_3", EjsValueType, EjsValueType, llvm.Type.getInt32Ty(), EjsValueType, EjsValueType, EjsValueType
                        ]
                        makeClosure: module.getOrInsertExternalFunction "_ejs_closure_new", EjsValueType, EjsClosureEnvType, EjsFuncType
                }
                
                @ejs = {
                        object_new:      module.getOrInsertExternalFunction "_ejs_object_new", EjsValueType, EjsValueType
                        number_new:      module.getOrInsertExternalFunction "_ejs_number_new", EjsValueType, llvm.Type.getDoubleTy()
                        boolean_new:     module.getOrInsertExternalFunction "_ejs_boolean_new", EjsValueType, boolType
                        string_new_utf8: module.getOrInsertExternalFunction "_ejs_string_new_utf8", EjsValueType, stringType
                        print:           module.getOrInsertGlobal "_ejs_print", EjsValueType
                        undefined:       module.getOrInsertGlobal "_ejs_undefined", EjsValueType
                        global:          module.getOrInsertGlobal "_ejs_global", EjsValueType
                        "unop!":         module.getOrInsertExternalFunction "_ejs_op_not", EjsValueType, EjsValueType
                        "unoptypeof":    module.getOrInsertExternalFunction "_ejs_op_typeof", EjsValueType, EjsValueType
                        "binop%":        module.getOrInsertExternalFunction "_ejs_op_mod", EjsValueType, EjsValueType, EjsValueType
                        "binop+":        module.getOrInsertExternalFunction "_ejs_op_add", EjsValueType, EjsValueType, EjsValueType
                        "binop<":        module.getOrInsertExternalFunction "_ejs_op_lt", EjsValueType, EjsValueType, EjsValueType
                        "binop-":        module.getOrInsertExternalFunction "_ejs_op_sub", EjsValueType, EjsValueType, EjsValueType
                        "binop===":      module.getOrInsertExternalFunction "_ejs_op_strict_eq", EjsValueType, EjsValueType, EjsValueType
                        truthy:          module.getOrInsertExternalFunction "_ejs_truthy", boolType, EjsValueType
                        object_setprop:  module.getOrInsertExternalFunction "_ejs_object_setprop", EjsValueType, EjsValueType, EjsValueType, EjsValueType
                        object_getprop:  module.getOrInsertExternalFunction "_ejs_object_getprop", EjsValueType, EjsValueType, EjsValueType
                }

        loadNullEjsValue: -> llvm.Constant.getNull EjsValueType
        loadUndefinedEjsValue: -> llvm.IRBuilder.createLoad @ejs.undefined, "load_undefined"                
        loadGlobal: -> llvm.IRBuilder.createLoad @ejs.global, "load_global"
        
        pushScope: (new_scope) ->
                new_scope[PARENT_SCOPE_KEY] = @current_scope
                @current_scope = new_scope

        popScope: ->
                @current_scope = @current_scope[PARENT_SCOPE_KEY]

        visitWithScope: (scope, children) ->
                @pushScope scope
                @visit child for child in children
                @popScope()

        findIdentifierInScope: (ident, scope) ->
                while scope?
                        if scope[ident]?
                                return scope[ident]
                        scope = scope[PARENT_SCOPE_KEY]
                return null

                                
        createAlloca: (func, type, name) ->
                saved_insert_point = llvm.IRBuilder.getInsertBlock()
                llvm.IRBuilder.setInsertPointStartBB func.entry_bb
                alloca = llvm.IRBuilder.createAlloca type, name
                llvm.IRBuilder.setInsertPoint saved_insert_point
                alloca

        createAllocas: (func, names, scope) ->
            allocas = []

            # the allocas are always allocated in the function entry_bb so the mem2reg opt pass can regenerate the ssa form for us
            saved_insert_point = llvm.IRBuilder.getInsertBlock()
            llvm.IRBuilder.setInsertPointStartBB func.entry_bb

            j = 0
            for i in [0..names.length-1]
                name = names[i].id.name
                if !scope[name]
                    allocas[j] = llvm.IRBuilder.createAlloca EjsValueType, "local_#{name}"
                    scope[name] = allocas[j]
                    j++

            # reinstate the IRBuilder to its previous insert point so we can insert the actual initializations
            llvm.IRBuilder.setInsertPoint saved_insert_point

            allocas

        createPropertyStore: (obj,propname,rhs) ->
                # we assume propname is a identifier here...
                pname = propname.name
                debug.log "createPropertyStore #{obj}.#{pname}"

                c = llvm.IRBuilder.createGlobalStringPtr pname, "strconst"
                strcall = llvm.IRBuilder.createCall @ejs.string_new_utf8, [c], "strtmp"
                                
                llvm.IRBuilder.createCall @ejs.object_setprop, [obj, strcall, rhs], "propstore_#{pname}"

        createPropertyLoad: (obj,propname) ->
                # we assume propname is a identifier here...
                pname = propname.name

                c = llvm.IRBuilder.createGlobalStringPtr pname, "strconst"
                strcall = llvm.IRBuilder.createCall @ejs.string_new_utf8, [c], "strtmp"
                
                return llvm.IRBuilder.createCall @ejs.object_getprop, [obj, strcall], "getprop_#{pname}"

        createLoadThis: () ->
                _this = @findIdentifierInScope EJS_THIS_NAME, @current_scope
                return llvm.IRBuilder.createLoad _this, "load_this"


        visitOrNull: (n) -> (@visit n) || @loadNullEjsValue()
        visitOrUndefined: (n) -> (@visit n) || @loadUndefinedEjsValue()
        


        visitProgram: (n) ->
                # by the time we make it here the program has been
                # transformed so that there is nothing at the toplevel
                # but function declarations.
                @visit func for func in n.body

        visitBlock: (n) ->
                new_scope = {}
                @visitWithScope new_scope, n.body
                n

        visitFor: (n) ->
                debug.log "visitFor"
                insertBlock = llvm.IRBuilder.getInsertBlock()
                insertFunc = insertBlock.parent

                init_bb = new llvm.BasicBlock "for_init", insertFunc
                test_bb = new llvm.BasicBlock "for_test", insertFunc
                body_bb = new llvm.BasicBlock "for_body", insertFunc
                merge_bb = new llvm.BasicBlock "for_merge", insertFunc

                llvm.IRBuilder.createBr init_bb
                
                llvm.IRBuilder.setInsertPoint init_bb
                @visit n.init
                llvm.IRBuilder.createBr test_bb

                llvm.IRBuilder.setInsertPoint test_bb
                cond_truthy = llvm.IRBuilder.createCall @ejs.truthy, [@visit(n.test)], "cond_truthy"
                cmp = llvm.IRBuilder.createICmpEq cond_truthy, (llvm.Constant.getIntegerValue boolType, 0), "cmpresult"
                llvm.IRBuilder.createCondBr cmp, merge_bb, body_bb

                llvm.IRBuilder.setInsertPoint body_bb
                @visit n.body
                @visit n.update
                llvm.IRBuilder.createBr test_bb
                
                llvm.IRBuilder.setInsertPoint merge_bb
                merge_bb
                
                
        visitWhile: (n) ->
                insertBlock = llvm.IRBuilder.getInsertBlock()
                insertFunc = insertBlock.parent
                
                while_bb  = new llvm.BasicBlock "while_start", insertFunc
                body_bb = new llvm.BasicBlock "while_body", insertFunc
                merge_bb = new llvm.BasicBlock "while_merge", insertFunc

                llvm.IRBuilder.createBr while_bb
                llvm.IRBuilder.setInsertPoint while_bb
                
                cond_truthy = llvm.IRBuilder.createCall @ejs.truthy, [@visit(n.test)], "cond_truthy"
                cmp = llvm.IRBuilder.createICmpEq cond_truthy, (llvm.Constant.getIntegerValue boolType, 0), "cmpresult"
                
                llvm.IRBuilder.createCondBr cmp, merge_bb, body_bb

                llvm.IRBuilder.setInsertPoint body_bb
                @visit n.body
                llvm.IRBuilder.createBr while_bb
                
                llvm.IRBuilder.setInsertPoint merge_bb
                merge_bb
                
        
                                
        visitIf: (n) ->
                # first we convert our conditional EJSValue to a boolean
                cond_truthy = llvm.IRBuilder.createCall @ejs.truthy, [@visit(n.test)], "cond_truthy"

                insertBlock = llvm.IRBuilder.getInsertBlock()
                insertFunc = insertBlock.parent

                then_bb  = new llvm.BasicBlock "then", insertFunc
                else_bb  = new llvm.BasicBlock "else", insertFunc
                merge_bb = new llvm.BasicBlock "merge", insertFunc

                # we invert the test here - check if the condition is false/0
                cmp = llvm.IRBuilder.createICmpEq cond_truthy, (llvm.Constant.getIntegerValue boolType, 0), "cmpresult"
                llvm.IRBuilder.createCondBr cmp, else_bb, then_bb

                llvm.IRBuilder.setInsertPoint then_bb
                then_val = @visit n.consequent
                llvm.IRBuilder.createBr merge_bb

                llvm.IRBuilder.setInsertPoint else_bb
                else_val = @visit n.alternate
                llvm.IRBuilder.createBr merge_bb

                llvm.IRBuilder.setInsertPoint merge_bb
                merge_bb
                
        visitReturn: (n) ->
                debug.log "visitReturn"
                if n.argument
                        llvm.IRBuilder.createRet @visit n.argument
                else
                        llvm.IRBuilder.createRet @loadNullEjsValue()
                        

        visitVariableDeclaration: (n) ->
                                
                if n.kind is "var"
                        # vars are hoisted to the containing function's toplevel scope
                        scope = @currentFunction.topScope

                        allocas = @createAllocas @currentFunction, n.declarations, scope
                        for i in [0..n.declarations.length-1]
                                initializer = @visitOrUndefined n.declarations[i].init
                                llvm.IRBuilder.createStore initializer, allocas[i]
                else if n.kind is "let"
                        # lets are not hoisted to the containing function's toplevel, but instead are bound in the lexical block they inhabit
                        scope = @current_scope;

                        allocas = @createAllocas @currentFunction, n.declarations.length, scope
                        for i in [0..n.declarations.length-1]
                                initializer = @visitOrUndefined n.declarations[i].init
                                llvm.IRBuilder.createStore initializer, allocas[i]
                else if n.kind is "const"
                        for i in [0..n.declarations.length-1]
                                u = n.declarations[i]
                                initializer_ir = @visit u.init
                                # XXX bind the initializer to u.name in the current basic block and mark it as constant

        visitMemberExpression: (n) ->
                @createPropertyLoad (@visit n.object), n.property
                
        visitAssignmentExpression: (n) ->
                lhs = n.left
                rhs = n.right

                if lhs.type is syntax.Identifier
                        dest = @findIdentifierInScope lhs.name, @current_scope
                        rhvalue = @visit rhs
                        if dest?
                                result = llvm.IRBuilder.createStore rhvalue, dest
                        else
                                result = @createPropertyStore @loadGlobal(), lhs, rhvalue
                        result
                else if lhs.type is syntax.MemberExpression
                        return @createPropertyStore (@visit lhs.object), lhs.property, @visit rhs
                else
                        throw "unhandled assign lhs"

        visitFunction: (n) ->
                # save off the insert point so we can get back to it after generating this function
                insertBlock = llvm.IRBuilder.getInsertBlock()

                for param in n.params
                        if param.type isnt syntax.Identifier
                                debug.log "we don't handle destructured/defaulted parameters yet"
                                throw "we don't handle destructured/defaulted parameters yet"

                # XXX this methods needs to be augmented so that we can pass actual types (or the builtin args need
                # to be reflected in jsllvm.cpp too).  maybe we can pass the names to this method and it can do it all
                # there?

                debug.log param.llvm_type for param in n.params

                ir_func = n.ir_func
                ir_args = n.ir_func.args
                debug.log "ir_func = #{ir_func}"
                debug.log "params = #{n.params.length}"

                @currentFunction = ir_func

                # Create a new basic block to start insertion into.
                entry_bb = new llvm.BasicBlock "entry", ir_func
                llvm.IRBuilder.setInsertPoint entry_bb

                new_scope = {}

                # we save off the top scope and entry_bb of the function so that we can hoist vars there
                ir_func.topScope = new_scope
                ir_func.entry_bb = entry_bb


                allocas = []
                i = 0
                # store the arguments on the stack
                for param in n.params
                        debug.log "   param : #{param.name}"
                        if param.type is syntax.Identifier
                                allocas[i] = llvm.IRBuilder.createAlloca ir_func.type.getParamType(i), "local_#{param.name}"
                                i += 1
                        else
                                debug.log "we don't handle destructured args at the moment."
                                throw "we don't handle destructured args at the moment."

                i = 0
                for param in n.params
                        # store the allocas in the scope we're going to push onto the scope stack
                        new_scope[param.name] = allocas[i]

                        llvm.IRBuilder.createStore ir_args[i], allocas[i]
                        i += 1

                body_bb = new llvm.BasicBlock "body", ir_func
                llvm.IRBuilder.setInsertPoint body_bb

                @visitWithScope new_scope, [n.body]

                # XXX more needed here - this lacks all sorts of control flow stuff.
                # Finish off the function.
                llvm.IRBuilder.createRet @loadNullEjsValue()

                # insert an unconditional branch from entry_bb to body here, now that we're
                # sure we're not going to be inserting allocas into the entry_bb anymore.
                llvm.IRBuilder.setInsertPoint entry_bb
                llvm.IRBuilder.createBr body_bb

                @currentFunction = null

                llvm.IRBuilder.setInsertPoint insertBlock

                return ir_func

        visitUnaryExpression: (n) ->
                debug.log "operator = '#{n.operator}'"
                builtin = "unop#{n.operator}"
                callee = @ejs[builtin]
                if not callee
                        throw "Internal error: unary operator '#{n.operator}' not implemented"
                return llvm.IRBuilder.createCall callee, [@visitOrNull n.argument], "result"
                
                
        visitBinaryExpression: (n) ->
                debug.log "operator = '#{n.operator}'"
                builtin = "binop#{n.operator}"
                callee = @ejs[builtin]
                if not callee
                        throw "Internal error: unhandled binary operator '#{n.operator}'"
                # call the add method
                return llvm.IRBuilder.createCall callee, [(@visit n.left), (@visit n.right)], "result_#{builtin}"

        visitLogicalExpression: (n) ->
                debug.log "operator = '#{n.operator}'"
                result = @createAlloca @currentFunction, EjsValueType, "result_#{n.operator}"

                left_visited = @visit n.left
                cond_truthy = llvm.IRBuilder.createCall @ejs.truthy, [left_visited], "cond_truthy"

                insertBlock = llvm.IRBuilder.getInsertBlock()
                insertFunc = insertBlock.parent
                
                left_bb  = new llvm.BasicBlock "cond_left", insertFunc
                right_bb  = new llvm.BasicBlock "cond_right", insertFunc
                merge_bb = new llvm.BasicBlock "cond_merge", insertFunc

                # we invert the test here - check if the condition is false/0
                cmp = llvm.IRBuilder.createICmpEq cond_truthy, (llvm.Constant.getIntegerValue boolType, 0), "cmpresult"
                llvm.IRBuilder.createCondBr cmp, right_bb, left_bb

                llvm.IRBuilder.setInsertPoint left_bb
                # inside the then branch, left was truthy
                if n.operator is "||"
                        # for || we short circuit out here
                        llvm.IRBuilder.createStore left_visited, result
                else if n.operator is "&&"
                        # for && we evaluate the second and store it
                        llvm.IRBuilder.createStore (@visit n.right), result
                else
                        throw "Internal error 99.1"
                llvm.IRBuilder.createBr merge_bb

                llvm.IRBuilder.setInsertPoint right_bb
                llvm.IRBuilder.createStore (@visit n.right), result
                llvm.IRBuilder.createBr merge_bb

                llvm.IRBuilder.setInsertPoint merge_bb
                rv = llvm.IRBuilder.createLoad result, "result_#{n.operator}_load"

                llvm.IRBuilder.setInsertPoint merge_bb

                rv

        visitCallExpression: (n) ->
                debug.log "visitCall #{JSON.stringify n}"

                args = n.arguments

                if n.callee.type is syntax.Identifier and n.callee.name[0] == '%'
                        callee = @builtins[n.callee.name.slice(1)]
                        if callee.length  # replace with a better Array test
                                callee = callee[n.arguments.length - BUILTIN_ARGS.length]
                else if n.callee.type is syntax.MemberExpression
                        debug.log "creating property load!"
                        callee = @createPropertyLoad n.callee.object, n.callee.property
                else
                        callee = @visit n.callee

                if not callee
                        throw "Internal error: callee should not be null"
                        
                # At this point we assume callee is a function object
                argv = []
                i = 0
                debug.log "args!!!!!!!!!!!!!!!!!!! #{args.length} of them"
                for arg in args
                        param_type = callee.type.getParamType(i)
                        debug.log "#{param_type}"
                        if param_type.toString() is "%EjsValue*"
                                debug.log 1
                                argv[i] = @visitOrNull arg
                        else if param_type.toString() is "%EjsClosureEnvType*"
                                debug.log 2
                                debug.log arg
                                argv[i] = @visitOrNull arg
                                debug.log argv[i]
                        else if param_type.toString() is "i32"
                                debug.log 4
                                argv[i] = llvm.Constant.getIntegerValue param_type, arg.value # XXX this is a shitty way to do type conversion
                                debug.log argv[i].toString()
                        else if param_type.toString() is EjsFuncType.toString()
                                debug.log 5
                                debug.log arg
                                f = @visit arg || throw "Internal error: callee should not be null"
                                debug.log 5.5
                                debug.log "f = #{f}"
                                argv[i] = llvm.IRBuilder.createPointerCast f, EjsFuncType, "func_cast"
                                debug.log 6
                        else
                                throw "Unhandled case, param_type = #{param_type}"
                        i += 1

                debug.log "done visiting args"

                # we're dealing with a function here
                # if callee.argSize isnt args.length
                # this isn't invalid in JS.  if argSize > args.length, the args are undefined.
                # if argSize < args.length, the args are still passed

                debug.log "callee == #{callee}"
                debug.log "  argSize = #{callee.argSize}"
                debug.log "  argv.length = #{argv.length}"
                (debug.log "arg:  #{arg}") for arg in argv

                return llvm.IRBuilder.createCall callee, argv, if callee.returnType and callee.returnType.isVoid() then "" else "calltmp"

        visitNewExpression: (n) ->
                ctor = @visit n.callee
                args = n.arguments

                # At this point we assume callee is a function object
                # if callee.argSize isnt args.length
                        # this isn't invalid in JS.  if argSize > args.length, the args are undefined.
                        # if argSize < args.length, the args are still passed

                argv = []
                for i in [0..args.length-1]
                        argv[i] = @visit args[i]

                llvm.IRBuilder.createCall callee, argv, "newtmp"

        visitPropertyAccess: (n) ->
                debug.log "property access: #{nc[1].value}" #NC-USAGE
                throw "whu"

        visitThisExpression: (n) ->
                debug.log "visitThisExpression"
                @createLoadThis()

        visitIdentifier: (n) ->
                debug.log "identifier #{n.name}"
                val = n.name
                alloca = @findIdentifierInScope val, @current_scope
                if alloca?
                        rv = llvm.IRBuilder.createLoad alloca, "load_#{val}"
                        return rv

                rv = null
                # we should probably insert a global scope at the toplevel (above the actual user-level global scope) that includes all these renamings/functions?
                if val is "print"
                        rv = llvm.IRBuilder.createLoad @ejs.print, "load_print"
                else if val is "undefined"
                        rv = llvm.IRBuilder.createLoad @ejs.undefined, "load_undefined"
                else
                        debug.log "calling getFunction for #{val}"
                        rv = @module.getFunction val

                if not rv
                        debug.log "Symbol '#{val}' not found in current scope"
                        rv = @createPropertyLoad @loadGlobal(), n

                debug.log "returning #{rv}"
                rv

        visitObjectExpression: (n) ->
                obj = llvm.IRBuilder.createCall @ejs.object_new, [@loadNullEjsValue()], "objtmp"
                for property in n.properties
                        key = property.key
                        val = @visit property.value
                        @createPropertyStore obj, key, val
                return obj

        visitArrayExpression: (n) ->
                throw new "Internal error: array expressions unsupported"
                
        visitExpressionStatement: (n) ->
                @visit n.expression

        visitLiteral: (n) ->
                if n.value is null
                        debug.log "literal: null"
                        return @loadNullEjsValue() # this isn't properly typed...  dunno what to do about this here
                else if typeof n.value is "string"
                        debug.log "literal string: #{n.value}"
                        c = llvm.IRBuilder.createGlobalStringPtr n.value, "strconst"
                        strcall = llvm.IRBuilder.createCall @ejs.string_new_utf8, [c], "strtmp"
                        debug.log "string_new_utf8 = #{@ejs.string_new_utf8}"
                        debug.log "strcall = #{strcall}"
                        return strcall
                else if typeof n.value is "number"
                        debug.log "literal number: #{n.value}"
                        c = llvm.ConstantFP.getDouble n.value
                        return llvm.IRBuilder.createCall @ejs.number_new, [c], "numtmp"
                else if typeof n.value is "boolean"
                        debug.log "literal boolean: #{n.value}"
                        c = llvm.Constant.getIntegerValue boolType, (if n.value then 1 else 0)
                        return llvm.IRBuilder.createCall @ejs.boolean_new, [c], "booltmp"
                throw "Internal error: unrecognized literal of type #{typeof n.value}"

class AddFunctionsVisitor extends NodeVisitor
        constructor: (@module) ->
                super

        visitFunction: (n) ->
                n.ir_name = "_ejs_anonymous"
                if n?.id?.name?
                        n.ir_name = n.id.name

                debug.log "n.params.length is #{n.params.length}"
                for param in n.params
                        debug.log "   param : #{param.name}"
                        param.llvm_type = EjsValueType
                        
                debug.log "BUILTIN_ARGS.length = #{BUILTIN_ARGS.length}"

                for i in [0..(BUILTIN_ARGS.length - 1)]
                        n.params[i].llvm_type = BUILTIN_ARGS[i].llvm_type
                
                n.ir_func = @module.getOrInsertFunction n.ir_name, EjsValueType, (param.llvm_type for param in n.params)
                debug.log "ADDFUNCTIONVISITOR:     ########## #{n.ir_name}"
                debug.log "      #{n.ir_func}"

                ir_args = n.ir_func.args
                i = 0
                for param in n.params
                        if param.type is syntax.Identifier
                                ir_args[i].setName param.name
                        else
                                ir_args[i].setName "__ejs_destructured_param"
                        i += 1


                # we don't need to recurse here since we won't have nested functions at this point
                n
                        
insert_toplevel_func = (tree) ->
        module_name = "testmodule"
        toplevel =
                type: syntax.FunctionDeclaration,
                id:
                        type: syntax.Identifier
                        name: "_ejs_script"    #   TODO this needs to be something like "__ejs_toplevel_#{module_name}" so we can compile multiple files
                params: [
                        { type: syntax.Identifier, name: "%env" }
                        { type: syntax.Identifier, name: "%argc" }
                ]
                body:
                        type: syntax.BlockStatement
                        body: tree.body
        tree.body = [toplevel]
        tree

debug.setLevel 0

exports.compile = (tree) ->

        tree = insert_toplevel_func tree
        
        tree = closure_conversion.convert tree

        debug.log -> escodegen.generate tree
        
        module = new llvm.Module "compiledfoo"

        visitor = new AddFunctionsVisitor module
        tree = visitor.visit tree

        debug.log -> escodegen.generate tree

        visitor = new LLVMIRVisitor module
        visitor.visit tree

        module
