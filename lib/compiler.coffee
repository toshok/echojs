esprima = require 'esprima'
syntax = esprima.Syntax
debug = require 'debug'

debug.setLevel 0

{ Set } = require 'set'
{ NodeVisitor } = require 'nodevisitor'
closure_conversion = require 'closure-conversion'

llvm = require 'llvm'

# special key for parent scope when performing lookups
PARENT_SCOPE_KEY = ":parent:"

EJS_CONTEXT_NAME = "__ejs_context"
EJS_THIS_NAME = "__ejs_this"
EJS_ENV = "__ejs_env"

EjsContextType = llvm.Type.getInt32Ty().pointerTo
EjsValueType = llvm.Type.getInt32Ty().pointerTo
EjsFuncType = (llvm.FunctionType.get EjsValueType, [EjsContextType, EjsValueType, llvm.Type.getInt32Ty()]).pointerTo

stringType = llvm.Type.getInt8Ty().pointerTo
boolType = llvm.Type.getInt8Ty()
voidType = llvm.Type.getVoidTy()

BUILTIN_ARGS = [
  { name: EJS_CONTEXT_NAME, llvm_type: EjsContextType, type: syntax.Identifier }
  { name: EJS_THIS_NAME,    llvm_type: EjsValueType, type: syntax.Identifier }
  { name: EJS_ENV,          llvm_type: EjsValueType, type: syntax.Identifier }
]

# marks an llvm function using its calling convention -- if ffi = true, we don't insert the 2 special ejs args into the call
ffi_c = (func) ->
  func.ffi = true
  func.ffi_type = "C"
  return func

ffi_objc = (func) ->
  func.ffi = true
  func.ffi_type = "ObjC"
  return func


class LLVMIRVisitor extends NodeVisitor
        constructor: (@module) ->
                @current_scope = {}

                # build up our runtime method table
                @ejs = {
                    invoke_closure: [
                        ffi_c module.getOrInsertExternalFunction "_ejs_invoke_closure_0", EjsValueType, EjsContextType, EjsValueType, llvm.Type.getInt32Ty()
                        ffi_c module.getOrInsertExternalFunction "_ejs_invoke_closure_1", EjsValueType, EjsContextType, EjsValueType, llvm.Type.getInt32Ty(), EjsValueType
                        ffi_c module.getOrInsertExternalFunction "_ejs_invoke_closure_2", EjsValueType, EjsContextType, EjsValueType, llvm.Type.getInt32Ty(), EjsValueType, EjsValueType
                        ffi_c module.getOrInsertExternalFunction "_ejs_invoke_closure_3", EjsValueType, EjsContextType, EjsValueType, llvm.Type.getInt32Ty(), EjsValueType, EjsValueType, EjsValueType
                    ]
                    number_new:      ffi_c module.getOrInsertExternalFunction "_ejs_number_new", EjsValueType, llvm.Type.getDoubleTy()
                    string_new_utf8: ffi_c module.getOrInsertExternalFunction "_ejs_string_new_utf8", EjsValueType, stringType
                    closure_new:     ffi_c module.getOrInsertExternalFunction "_ejs_closure_new", EjsValueType, EjsFuncType, EjsValueType
                    print:           ffi_c module.getOrInsertExternalFunction "_ejs_print", voidType, EjsValueType
                    "binop+":        ffi_c module.getOrInsertExternalFunction "_ejs_op_add", boolType, EjsValueType, EjsValueType, EjsValueType.pointerTo
                    "binop-":        ffi_c module.getOrInsertExternalFunction "_ejs_op_sub", boolType, EjsValueType, EjsValueType, EjsValueType.pointerTo
                    "logop||":       ffi_c module.getOrInsertExternalFunction "_ejs_op_or", boolType, EjsValueType, EjsValueType, EjsValueType.pointerTo
                    "binop===":      ffi_c module.getOrInsertExternalFunction "_ejs_op_strict_eq", boolType, EjsValueType, EjsValueType, EjsValueType.pointerTo
                    truthy:          ffi_c module.getOrInsertExternalFunction "_ejs_truthy", boolType, EjsValueType, boolType.pointerTo
                    object_setprop:  ffi_c module.getOrInsertExternalFunction "_ejs_object_setprop", boolType, EjsValueType, EjsValueType, EjsValueType
                    object_getprop:  ffi_c module.getOrInsertExternalFunction "_ejs_object_getprop", boolType, EjsValueType, EjsValueType, EjsValueType.pointerTo
                }

        pushScope: (new_scope) ->
                new_scope[PARENT_SCOPE_KEY] = @current_scope
                @current_scope = new_scope

        popScope: ->
                @current_scope = @current_scope[PARENT_SCOPE_KEY]

        createAlloca: (func, type, name) ->
                saved_insert_point = llvm.IRBuilder.getInsertBlock()
                llvm.IRBuilder.setInsertPoint func.entry_bb
                alloca = llvm.IRBuilder.createAlloca type, name
                llvm.IRBuilder.setInsertPoint saved_insert_point
                alloca

        createAllocas: (func, names, scope) ->
            allocas = []

            # the allocas are always allocated in the function entry_bb so the mem2reg opt pass can regenerate the ssa form for us
            saved_insert_point = llvm.IRBuilder.getInsertBlock()
            llvm.IRBuilder.setInsertPoint func.entry_bb

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

        visitProgram: (n) ->
                funcs = []
                nonfunc = []

                for child in n.body
                        if child.type is syntax.FunctionDeclaration
                                funcs.push child
                        else
                                nonfunc.push child

                # generate the IR for all the functions first
                @visit func for func in funcs

                ir_func = @currentFunction
                ir_args = ir_func.args
                if top?
                        # XXX this block needs reworking to mirror what happens in visitFunction (particularly the distinction between entry/body_bb)
                        @currentFunction = ir_func
                        # Create a new basic block to start insertion into.
                        entry_bb = new llvm.BasicBlock "entry", ir_func
                        llvm.IRBuilder.setInsertPoint entry_bb
                        ir_func.entry_bb = entry_bb
                        new_scope = {}
                        ir_func.topScope = new_scope
                        @current_scope = new_scope

                allocas = []
                for i in [0..BUILTIN_ARGS.length-1]
                        ir_args[i].setName BUILTIN_ARGS[i].name
                        allocas[i] = llvm.IRBuilder.createAlloca EjsValueType, "local_#{BUILTIN_ARGS[i].name}"
                        new_scope[BUILTIN_ARGS[i].name] = allocas[i]
                        llvm.IRBuilder.createStore ir_args[i], allocas[i]

                for i in [0..nonfunc.length-1]
                        ir = @visit nonfunc[i]

                ir_func

        visitBlock: (n) ->
                new_scope = {}

                @pushScope new_scope

                debug.log "hi22"
                @visit statement for statement in n.body
                debug.log "hi23"
                
                @popScope()

                n

        visitIf: (n) ->
                # first we convert our conditional EJSValue to a boolean
                truthy_stackalloc = @createAlloca @currentFunction, boolType, "truthy_result"
                llvm.IRBuilder.createCall @ejs.truthy, [@visit(n.test), truthy_stackalloc], "cond_truthy"
                cond_truthy = llvm.IRBuilder.createLoad truthy_stackalloc, "truthy_load"

                insertBlock = llvm.IRBuilder.getInsertBlock()
                insertFunc = insertBlock.parent

                then_bb  = new llvm.BasicBlock "then", insertFunc
                else_bb  = new llvm.BasicBlock "else", insertFunc
                merge_bb = new llvm.BasicBlock "merge", insertFunc

                # we invert the test here - check if the condition is false/0
                cmp = llvm.IRBuilder.createICmpEq cond_truthy, (llvm.Constant.getIntegerValue boolType, 0), "cmpresult"
                llvm.IRBuilder.createCondBr cmp, else_bb, then_bb

                llvm.IRBuilder.setInsertPoint then_bb
                then_val = @visit n.thenPart
                llvm.IRBuilder.createBr merge_bb

                llvm.IRBuilder.setInsertPoint else_bb
                else_val = @visit n.elsePart

                llvm.IRBuilder.setInsertPoint merge_bb
                merge_bb
                
        visitReturn: (n) ->
                debug.log "visitReturn"
                llvm.IRBuilder.createRet (@visit n.argument)

        visitVariableDeclaration: (n) ->
                                
                if n.kind is "var"
                        # vars are hoisted to the containing function's toplevel scope
                        scope = @currentFunction.topScope

                        allocas = @createAllocas @currentFunction, n.declarations, scope
                        for i in [0..n.declarations.length-1]
                                initializer = @visit n.declarations[i].init
                                llvm.IRBuilder.createStore initializer, allocas[i]
                else if n.kind is "let"
                        # lets are not hoisted to the containing function's toplevel, but instead are bound in the lexical block they inhabit
                        scope = @current_scope;

                        allocas = @createAllocas @currentFunction, n.declarations.length, scope
                        for i in [0..n.declarations.length-1]
                                llvm.IRBuilder.createStore (@visit n.declarations[i].init), allocas[i]
                else if n.kind is "const"
                        for i in [0..n.declarations.length-1]
                                u = n.declarations[i]
                                initializer_ir = @visit u.init
                                # XXX bind the initializer to u.name in the current basic block and mark it as constant

        createPropertyStore: (obj,propname,rhs) ->
                # we assume propname is a string/identifier here...
                pname = @visitString propname
                llvm.IRBuilder.createCall @ejs.object_setprop, [(@visit obj), pname, rhs], "propstore_#{propname.value}"

        createPropertyLoad: (obj,propname) ->
                # we assume propname is a string/identifier here...
                pname = @visitString propname
                result = @createAlloca EjsValueType, "result"
                rv = llvm.IRBuilder.createCall @ejs.object_getprop, [(@visit obj), pname, result], "propload_#{propname.value}"
                llvm.IRBuilder.createLoad result, "result_propload"

        visitAssignmentExpression: (n) ->
                lhs = n.left
                rhs = n.right

                if lhs.type is syntax.Identifier
                        result = llvm.IRBuilder.createStore (@visit rhs), (findIdentifierInScope lhs.value, @current_scope)
                        result;
                else if lhs.type is syntax.MemberExpression
                        return @createPropertyStore lhs.object, lhs.property, @visit rhs
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

                for i in [(BUILTIN_ARGS.length - 1)..0]
                  n.params.unshift BUILTIN_ARGS[i]

                name = "_ejs_anonymous"
                if n?.id?.name?
                        name = n.id.name
                ir_func = @module.getOrInsertFunction name, n.params.length
                @currentFunction = ir_func

                ir_args = ir_func.args
                i = 0
                for param in n.params
                        if param.type is syntax.Identifier
                                ir_args[i].setName param.name
                        else
                                ir_args[i].setName "__ejs_destructured_param"
                        i += 1

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
                        if param.type is syntax.Identifier
                                allocas[i] = llvm.IRBuilder.createAlloca EjsValueType, "local_#{param.name}"
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

                @pushScope new_scope

                @visit n.body

                @popScope()

                # XXX more needed here - this lacks all sorts of control flow stuff.
                # Finish off the function.
                llvm.IRBuilder.createRet (llvm.Constant.getNull EjsValueType)

                # insert an unconditional branch from entry_bb to body here, now that we're
                # sure we're not going to be inserting allocas into the entry_bb anymore.
                llvm.IRBuilder.setInsertPoint entry_bb
                llvm.IRBuilder.createBr body_bb

                @currentFunction = null

                llvm.IRBuilder.setInsertPoint insertBlock

                return ir_func

        visitBinaryExpression: (n) ->
                debug.log "operator = '#{n.operator}'"
                builtin = "binop#{n.operator}"
                callee = @ejs[builtin]
                # allocate space on the stack for the result
                result = @createAlloca @currentFunction, EjsValueType, "result_#{builtin}"
                args = [(@visit n.left), (@visit n.right), result]
                # call the add method
                rv = llvm.IRBuilder.createCall callee, args, "result"
                # load and return the result
                return llvm.IRBuilder.createLoad result, "result_#{builtin}_load"

        visitLogicalExpression: (n) ->
                debug.log "operator = '#{n.operator}'"
                builtin = "logop#{n.operator}"
                callee = @ejs[builtin]
                # allocate space on the stack for the result
                result = @createAlloca @currentFunction, EjsValueType, "result_#{builtin}"
                # call the add method
                rv = llvm.IRBuilder.createCall callee, [(@visit n.left), (@visit n.right), result], "result"
                # load and return the result
                return llvm.IRBuilder.createLoad result, "result_#{builtin}_load"

        createLoadThis: () ->
                _this = findIdentifierInScope EJS_THIS_NAME, @current_scope
                return llvm.IRBuilder.createLoad _this, "load_this"

        createLoadContext: () ->
                _context = findIdentifierInScope EJS_CONTEXT_NAME, @current_scope
                return llvm.IRBuilder.createLoad _context, "load_context"

        visitCallExpression: (n) ->
                debug.log "visitCall #{JSON.stringify n}"

                args = n.arguments

                if n.callee.type is syntax.MemberExpression
                        debug.log "creating property load!"
                        callee = @createPropertyLoad n.callee.object, n.callee.property
                else if n.callee.type is syntax.Identifier and n.callee.name is "%invokeClosure"
                        callee = @ejs.invoke_closure[n.arguments.length - 3]
                else
                        callee = @visit n.callee

                # At this point we assume callee is a function object

                argv = (@visit arg for arg in args)
                
                if not callee.ffi?
                        # we insert the extra BUILTIN_ARGS since we're calling a JS function
                        argv.unshift @createLoadThis()
                        argv.unshift @createLoadContext()
                        argv.unshift llvm.Constant.getNull EjsValueType

                        compiled_arg_start = 3
                debug.log "done visiting args"

                # we're dealing with a function here
                # if callee.argSize isnt args.length
                # this isn't invalid in JS.  if argSize > args.length, the args are undefined.
                # if argSize < args.length, the args are still passed

                arg.dump() for arg in argv
                
                debug.log "callee == #{callee}, argSize = #{callee.argSize}, argv.length = #{argv.length}"
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
                alloca = findIdentifierInScope val, @current_scope
                if alloca?
                        return llvm.IRBuilder.createLoad alloca, "load_#{val}"

                func = null

                # we should probably insert a global scope at the toplevel (above the actual user-level global scope) that includes all these renamings/functions?
                if val is "print"
                        func = @ejs.print
                else
                        func = @module.getFunction val

                if func?
                        return func

                throw "Symbol '#{val}' not found in current scope"


        visitExpressionStatement: (n) ->
                @visit n.expression

        visitLiteral: (n) ->
                if n.value is null
                        debug.log "literal: null"
                        return llvm.Constant.getNull EjsValueType # this isn't properly typed...  dunno what to do about this here
                else if typeof n.value is "string"
                        debug.log "literal string: #{n.value}"
                        c = llvm.IRBuilder.createGlobalStringPtr n.value, "strconst"
                        return llvm.IRBuilder.createCall @ejs.string_new_utf8, [c], "strtmp"
                else if typeof n.value is "number"
                        debug.log "literal number: #{n.value}"
                        c = llvm.ConstantFP.getDouble n.value
                        return llvm.IRBuilder.createCall @ejs.number_new, [c], "numtmp"
                throw "Internal error"

findIdentifierInScope = (ident, scope) ->
        while scope?
                if scope[ident]?
                        return scope[ident]
                scope = scope[PARENT_SCOPE_KEY]
        return null

insert_toplevel_func = (tree) ->
        module_name = "testmodule"
        toplevel =
                type: syntax.FunctionDeclaration,
                id:
                        type: syntax.Identifier
                        name: "__ejs_toplevel_#{module_name}",
                params: [] # TODO we need the toplevel parameters (context?) here
                body:
                        type: syntax.BlockStatement
                        body: tree.body
        tree.body = [toplevel]
        tree

debug.setLevel 0

exports.compile = (tree) ->

        tree = insert_toplevel_func tree
        
        tree = closure_conversion.convert tree
                
        module = new llvm.Module "compiledfoo"
        
        visitor = new LLVMIRVisitor module
        visitor.visit tree

        module
