esprima = require 'esprima'
escodegen = require 'escodegen'
syntax = esprima.Syntax
debug = require 'debug'

debug.setLevel 0

{ Set } = require 'set'
{ NodeVisitor } = require 'nodevisitor'
closure_conversion = require 'closure-conversion'
{ desugar } = require 'echo-desugar'

llvm = require 'llvm'

# special key for parent scope when performing lookups
PARENT_SCOPE_KEY = ":parent:"

stringType = llvm.Type.getInt8Ty().pointerTo()
boolType   = llvm.Type.getInt8Ty()
voidType   = llvm.Type.getVoidTy()
int32Type  = llvm.Type.getInt32Ty()
int64Type  = llvm.Type.getInt64Ty()

ejsValueType = llvm.StructType.create "EjsValue", [int32Type]

EjsValueType = ejsValueType.pointerTo()
EjsClosureEnvType = EjsValueType
EjsPropIteratorType = EjsValueType
EjsClosureFuncType = (llvm.FunctionType.get EjsValueType, [EjsClosureEnvType, EjsValueType, llvm.Type.getInt32Ty(), EjsValueType.pointerTo()]).pointerTo()

BUILTIN_PARAMS = [
  { type: syntax.Identifier, name: "%env",  llvm_type: EjsClosureEnvType }
  { type: syntax.Identifier, name: "%this", llvm_type: EjsValueType }
  { type: syntax.Identifier, name: "%argc", llvm_type: int32Type }
]

takes_builtins = (n) ->
        n.takes_builtins = true
        n
        
class LLVMIRVisitor extends NodeVisitor
        constructor: (@module) ->

                make_invoke_closure = (n) ->
                        takes_builtins module.getOrInsertExternalFunction "_ejs_invoke_closure_#{n}", EjsValueType, [EjsValueType, EjsValueType, llvm.Type.getInt32Ty()].concat (EjsValueType for i in [0...n])
                        
                # build up our runtime method table
                @builtins = {
                        invokeClosure: [null].concat (make_invoke_closure n for n in [0..10])
                        makeClosure: module.getOrInsertExternalFunction "_ejs_function_new", EjsValueType, [EjsClosureEnvType, EjsValueType, EjsClosureFuncType]
                }
                
                @ejs = {
                        object_new:            module.getOrInsertExternalFunction "_ejs_object_new",                EjsValueType, [EjsValueType]
                        arguments_new:         module.getOrInsertExternalFunction "_ejs_arguments_new",             EjsValueType, [int32Type, EjsValueType.pointerTo()]
                        array_new:             module.getOrInsertExternalFunction "_ejs_array_new",                 EjsValueType, [int32Type]
                        number_new:            module.getOrInsertExternalFunction "_ejs_number_new",                EjsValueType, [llvm.Type.getDoubleTy()]
                        boolean_new:           module.getOrInsertExternalFunction "_ejs_boolean_new",               EjsValueType, [boolType]
                        string_new_utf8:       module.getOrInsertExternalFunction "_ejs_string_new_utf8",           EjsValueType, [stringType]
                        regexp_new_utf8:       module.getOrInsertExternalFunction "_ejs_regexp_new_utf8",           EjsValueType, [stringType]
                        truthy:                module.getOrInsertExternalFunction "_ejs_truthy",                    boolType, [EjsValueType]
                        object_setprop:        module.getOrInsertExternalFunction "_ejs_object_setprop",            EjsValueType, [EjsValueType, EjsValueType, EjsValueType]
                        object_getprop:        module.getOrInsertExternalFunction "_ejs_object_getprop",            EjsValueType, [EjsValueType, EjsValueType]
                        object_getprop_utf8:   module.getOrInsertExternalFunction "_ejs_object_getprop_utf8",       EjsValueType, [EjsValueType, stringType]
                        object_setprop_utf8:   module.getOrInsertExternalFunction "_ejs_object_setprop_utf8",       EjsValueType, [EjsValueType, stringType, EjsValueType]
                        prop_iterator_new:     module.getOrInsertExternalFunction "_ejs_property_iterator_new",     EjsPropIteratorType, [EjsValueType]
                        prop_iterator_current: module.getOrInsertExternalFunction "_ejs_property_iterator_current", stringType, [EjsPropIteratorType]
                        prop_iterator_next:    module.getOrInsertExternalFunction "_ejs_property_iterator_next",    voidType, [EjsPropIteratorType]
                        prop_iterator_free:    module.getOrInsertExternalFunction "_ejs_property_iterator_free",    voidType, [EjsPropIteratorType]
                        undefined:             module.getOrInsertGlobal           "_ejs_undefined",                 EjsValueType
                        global:                module.getOrInsertGlobal           "_ejs_global",                    EjsValueType
                        
                        "unop-":           module.getOrInsertExternalFunction "_ejs_op_neg",         EjsValueType, [EjsValueType]
                        "unop+":           module.getOrInsertExternalFunction "_ejs_op_plus",        EjsValueType, [EjsValueType]
                        "unop!":           module.getOrInsertExternalFunction "_ejs_op_not",         EjsValueType, [EjsValueType]
                        "unoptypeof":      module.getOrInsertExternalFunction "_ejs_op_typeof",      EjsValueType, [EjsValueType]
                        "unopdelete":      module.getOrInsertExternalFunction "_ejs_op_delete",      EjsValueType, [EjsValueType, EjsValueType] # this is a unop, but ours only works for memberexpressions
                        "unopvoid":        module.getOrInsertExternalFunction "_ejs_op_void",        EjsValueType, [EjsValueType]
                        "binop&":          module.getOrInsertExternalFunction "_ejs_op_bitwise_and", EjsValueType, [EjsValueType, EjsValueType]
                        "binop|":          module.getOrInsertExternalFunction "_ejs_op_bitwise_or",  EjsValueType, [EjsValueType, EjsValueType]
                        "binop>>":         module.getOrInsertExternalFunction "_ejs_op_rsh",         EjsValueType, [EjsValueType, EjsValueType]
                        "binop>>>":        module.getOrInsertExternalFunction "_ejs_op_ursh",        EjsValueType, [EjsValueType, EjsValueType]
                        "binop%":          module.getOrInsertExternalFunction "_ejs_op_mod",         EjsValueType, [EjsValueType, EjsValueType]
                        "binop+":          module.getOrInsertExternalFunction "_ejs_op_add",         EjsValueType, [EjsValueType, EjsValueType]
                        "binop*":          module.getOrInsertExternalFunction "_ejs_op_mult",        EjsValueType, [EjsValueType, EjsValueType]
                        "binop/":          module.getOrInsertExternalFunction "_ejs_op_div",         EjsValueType, [EjsValueType, EjsValueType]
                        "binop<":          module.getOrInsertExternalFunction "_ejs_op_lt",          EjsValueType, [EjsValueType, EjsValueType]
                        "binop<=":         module.getOrInsertExternalFunction "_ejs_op_le",          EjsValueType, [EjsValueType, EjsValueType]
                        "binop>":          module.getOrInsertExternalFunction "_ejs_op_gt",          EjsValueType, [EjsValueType, EjsValueType]
                        "binop>=":         module.getOrInsertExternalFunction "_ejs_op_ge",          EjsValueType, [EjsValueType, EjsValueType]
                        "binop-":          module.getOrInsertExternalFunction "_ejs_op_sub",         EjsValueType, [EjsValueType, EjsValueType]
                        "binop===":        module.getOrInsertExternalFunction "_ejs_op_strict_eq",   EjsValueType, [EjsValueType, EjsValueType]
                        "binop==":         module.getOrInsertExternalFunction "_ejs_op_eq",          EjsValueType, [EjsValueType, EjsValueType]
                        "binop!==":        module.getOrInsertExternalFunction "_ejs_op_strict_neq",  EjsValueType, [EjsValueType, EjsValueType]
                        "binop!=":         module.getOrInsertExternalFunction "_ejs_op_neq",         EjsValueType, [EjsValueType, EjsValueType]
                        "binopinstanceof": module.getOrInsertExternalFunction "_ejs_op_instanceof",  EjsValueType, [EjsValueType, EjsValueType]
                        "binopin":         module.getOrInsertExternalFunction "_ejs_op_in",          EjsValueType, [EjsValueType, EjsValueType]
                }

                @initGlobalScope();

        loadNullEjsValue: -> llvm.Constant.getNull EjsValueType
        loadUndefinedEjsValue: ->
                undef = llvm.IRBuilder.createLoad @ejs.undefined, "load_undefined"
                undef.isundefined = true
                undef
        loadGlobal: -> llvm.IRBuilder.createLoad @ejs.global, "load_global"

        pushIIFEInfo: (info) ->
                @iifeStack.unshift info
                
        popIIFEInfo: ->
                @iifeStack.shift

        initGlobalScope: ->
                @current_scope = {}

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
                        if scope.hasOwnProperty ident
                                return scope[ident]
                        scope = scope[PARENT_SCOPE_KEY]
                return null

                                
        createAlloca: (func, type, name) ->
                saved_insert_point = llvm.IRBuilder.getInsertBlock()
                llvm.IRBuilder.setInsertPointStartBB func.entry_bb
                alloca = llvm.IRBuilder.createAlloca type, name
                llvm.IRBuilder.setInsertPoint saved_insert_point
                alloca

        createAllocas: (func, ids, scope) ->
                allocas = []
                new_allocas = []
                
                # the allocas are always allocated in the function entry_bb so the mem2reg opt pass can regenerate the ssa form for us
                saved_insert_point = llvm.IRBuilder.getInsertBlock()
                llvm.IRBuilder.setInsertPointStartBB func.entry_bb

                j = 0
                for i in [0...ids.length]
                        name = ids[i].id.name
                        if !scope.hasOwnProperty name
                                allocas[j] = llvm.IRBuilder.createAlloca EjsValueType, "local_#{name}"
                                scope[name] = allocas[j]
                                new_allocas[j] = true
                        else
                                allocas[j] = scope[name]
                                new_allocas[j] = false
                        j = j + 1
                                

                # reinstate the IRBuilder to its previous insert point so we can insert the actual initializations
                llvm.IRBuilder.setInsertPoint saved_insert_point

                { allocas: allocas, new_allocas: new_allocas }

        createPropertyStore: (obj,prop,rhs,computed) ->
                if computed
                        # we store obj[prop], prop can be any value
                        llvm.IRBuilder.createCall @ejs.object_setprop, [obj, (@visit prop), rhs], "propstore_computed"
                else
                        # we store obj.prop, prop is an id
                        if prop.type is syntax.Identifier
                                pname = prop.name
                        else # prop.type is syntax.Literal
                                pname = prop.value

                        debug.log "createPropertyStore #{obj}[#{pname}]"

                        c = llvm.IRBuilder.createGlobalStringPtr pname, "strconst"
                        llvm.IRBuilder.createCall @ejs.object_setprop_utf8, [obj, c, rhs], "propstore_#{pname}"
                
        createPropertyLoad: (obj,prop,computed) ->
                if computed
                        # we load obj[prop], prop can be any value
                        loadprop = @visit prop
                        pname = "computed"
                        llvm.IRBuilder.createCall @ejs.object_getprop, [obj, loadprop], "getprop_#{pname}"
                else
                        # we load obj.prop, prop is an id
                        pname = prop.name
                        c = llvm.IRBuilder.createGlobalStringPtr pname, "strconst"
                        llvm.IRBuilder.createCall @ejs.object_getprop_utf8, [obj, c], "getprop_#{pname}"
                

        createLoadThis: () ->
                _this = @findIdentifierInScope "%this", @current_scope
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

                iife_rv = null
                iife_bb = null
                
                if n.fromIIFE
                        insertBlock = llvm.IRBuilder.getInsertBlock()
                        insertFunc = insertBlock.parent
                        
                        iife_rv = @createAlloca @currentFunction, EjsValueType, "%iife_rv"
                        iife_bb = new llvm.BasicBlock "iife_dest", insertFunc

                @pushIIFEInfo iife_rv: iife_rv, iife_dest_bb: iife_bb

                @visitWithScope new_scope, n.body

                @popIIFEInfo()
                if iife_bb
                        llvm.IRBuilder.createBr iife_bb
                        llvm.IRBuilder.setInsertPoint iife_bb
                        rv = llvm.IRBuilder.createLoad iife_rv, "%iife_rv_load"
                        rv
                else
                        n

        visitLabeledStatement: (n) ->
                n.body.label = n.label.name
                @visit n.body

        visitBreak: (n) ->
                if not n.label
                        llvm.IRBuilder.createBr @breakStack[0].dest
                else
                        el = el for el in @breakStack when el.label is n.label.name
                        llvm.IRBuilder.createBr el.dest

                
        visitContinue: (n) ->
                if not n.label
                        llvm.IRBuilder.createBr @continueStack[0].dest
                else
                        el = el for el in @continueStack when el.label is n.label.name
                        llvm.IRBuilder.createBr el.dest

        visitFor: (n) ->
                insertBlock = llvm.IRBuilder.getInsertBlock()
                insertFunc = insertBlock.parent

                init_bb = new llvm.BasicBlock "for_init", insertFunc
                test_bb = new llvm.BasicBlock "for_test", insertFunc
                body_bb = new llvm.BasicBlock "for_body", insertFunc
                update_bb = new llvm.BasicBlock "for_update", insertFunc
                merge_bb = new llvm.BasicBlock "for_merge", insertFunc

                llvm.IRBuilder.createBr init_bb
                
                llvm.IRBuilder.setInsertPoint init_bb
                @visit n.init
                llvm.IRBuilder.createBr test_bb

                llvm.IRBuilder.setInsertPoint test_bb
                if n.test
                        cond_truthy = llvm.IRBuilder.createCall @ejs.truthy, [@visit(n.test)], "cond_truthy"
                        cmp = llvm.IRBuilder.createICmpEq cond_truthy, (llvm.Constant.getIntegerValue boolType, 0), "cmpresult"
                        llvm.IRBuilder.createCondBr cmp, merge_bb, body_bb
                else
                        llvm.IRBuilder.createBr body_bb

                @continueStack.unshift label: n.label, dest: update_bb
                @breakStack.unshift    label: n.label, dest: merge_bb
                
                llvm.IRBuilder.setInsertPoint body_bb
                @visit n.body
                llvm.IRBuilder.createBr update_bb

                llvm.IRBuilder.setInsertPoint update_bb
                @visit n.update
                llvm.IRBuilder.createBr test_bb
                
                @continueStack.shift()
                @breakStack.shift()

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

                @continueStack.unshift label: n.label, dest: while_bb
                @breakStack.unshift    label: n.label, dest: merge_bb
                
                llvm.IRBuilder.setInsertPoint body_bb
                @visit n.body
                llvm.IRBuilder.createBr while_bb

                @continueStack.shift()
                @breakStack.shift()
                                
                llvm.IRBuilder.setInsertPoint merge_bb
                merge_bb

        visitForIn: (n) ->
                insertBlock = llvm.IRBuilder.getInsertBlock()
                insertFunc = insertBlock.parent
                
                iterator = llvm.IRBuilder.createCall @ejs.prop_iterator_new, [@visit n.right], "iterator"

                # make sure we get an alloca if there's a "var"
                if n.left[0]?
                        @visit n.left
                        lhs = n.left[0].declarations[0].id
                else
                        lhs = n.left
                
                forin_bb  = new llvm.BasicBlock "forin_start", insertFunc
                body_bb   = new llvm.BasicBlock "forin_body",  insertFunc
                merge_bb  = new llvm.BasicBlock "forin_merge", insertFunc
                                
                llvm.IRBuilder.createBr forin_bb
                llvm.IRBuilder.setInsertPoint forin_bb

                current = llvm.IRBuilder.createCall @ejs.prop_iterator_current, [iterator], "iterator_current"

                cmp = llvm.IRBuilder.createICmpEq current, (llvm.Constant.getNull stringType), "cmpresult"
                
                llvm.IRBuilder.createCondBr cmp, merge_bb, body_bb

                llvm.IRBuilder.setInsertPoint body_bb
                strcall = llvm.IRBuilder.createCall @ejs.string_new_utf8, [current], "strtmp"
                @storeValueInDest strcall, lhs
                @visit n.body
                llvm.IRBuilder.createCall @ejs.prop_iterator_next, [iterator], ""
                llvm.IRBuilder.createBr forin_bb

                llvm.IRBuilder.setInsertPoint merge_bb
                llvm.IRBuilder.createCall @ejs.prop_iterator_free, [iterator], ""
                merge_bb
                
                
        visitUpdateExpression: (n) ->
                result = @createAlloca @currentFunction, EjsValueType, "%update_result"
                argument = @visit n.argument
                c = llvm.ConstantFP.getDouble 1
                num = llvm.IRBuilder.createCall @ejs.number_new, [c], "numtmp"
                if not n.prefix
                        # postfix updates store the argument before the op
                        llvm.IRBuilder.createStore argument, result

                # argument = argument $op 1
                temp = llvm.IRBuilder.createCall @ejs["binop#{if n.operator is '++' then '+' else '-'}"], [argument, num], "update_temp"
                
                @storeValueInDest temp, n.argument
                
                # return result
                if n.prefix
                        argument = @visit n.argument
                        # prefix updates store the argument after the op
                        llvm.IRBuilder.createStore argument, result
                llvm.IRBuilder.createLoad result, "%update_result_load"

        visitConditionalExpression: (n) ->
                @visitIfOrCondExp n, true
                        
        visitIf: (n) ->
                @visitIfOrCondExp n, false

        visitIfOrCondExp: (n, load_result) ->
                if load_result
                        cond_val = @createAlloca @currentFunction, EjsValueType, "%cond_val"
                        
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
                if load_result
                        llvm.IRBuilder.createStore then_val, cond_val
                llvm.IRBuilder.createBr merge_bb

                llvm.IRBuilder.setInsertPoint else_bb
                else_val = @visit n.alternate
                if load_result
                        llvm.IRBuilder.createStore else_val, cond_val
                llvm.IRBuilder.createBr merge_bb

                llvm.IRBuilder.setInsertPoint merge_bb
                if load_result
                        llvm.IRBuilder.createLoad cond_val, "cond_val_load"
                else
                        merge_bb
                
        visitReturn: (n) ->
                debug.log "visitReturn"
                if @iifeStack[0].iife_rv?
                        if n.argument?
                                llvm.IRBuilder.createStore (@visit n.argument), @iifeStack[0].iife_rv
                        else
                                llvm.IRBuilder.createStore @loadUndefinedEjsValue(), @iifeStack[0].iife_rv
                        llvm.IRBuilder.createBr @iifeStack[0].iife_dest_bb
                else
                        if n.argument
                                llvm.IRBuilder.createRet @visit n.argument
                        else
                                llvm.IRBuilder.createRet @loadUndefinedEjsValue()
                        

        visitVariableDeclaration: (n) ->
                                
                if n.kind is "var"
                        # vars are hoisted to the containing function's toplevel scope
                        scope = @currentFunction.topScope

                        {allocas,new_allocas} = @createAllocas @currentFunction, n.declarations, scope
                        for i in [0...n.declarations.length]
                                if not n.declarations[i].init?
                                        # there was not an initializer. we only store undefined
                                        # if the alloca is newly allocated.
                                        if new_allocas[i]
                                                initializer = @visitOrUndefined n.declarations[i].init
                                                llvm.IRBuilder.createStore initializer, allocas[i]
                                else
                                        initializer = @visitOrUndefined n.declarations[i].init
                                        llvm.IRBuilder.createStore initializer, allocas[i]
                else if n.kind is "let"
                        # lets are not hoisted to the containing function's toplevel, but instead are bound in the lexical block they inhabit
                        scope = @current_scope;

                        {allocas,new_allocas} = @createAllocas @currentFunction, n.declarations, scope
                        for i in [0...n.declarations.length]
                                if not n.declarations[i].init?
                                        # there was not an initializer. we only store undefined
                                        # if the alloca is newly allocated.
                                        if new_allocas[i]
                                                initializer = @visitOrUndefined n.declarations[i].init
                                                llvm.IRBuilder.createStore initializer, allocas[i]
                                else
                                        initializer = @visitOrUndefined n.declarations[i].init
                                        llvm.IRBuilder.createStore initializer, allocas[i]
                else if n.kind is "const"
                        for i in [0...n.declarations.length]
                                u = n.declarations[i]
                                initializer_ir = @visit u.init
                                # XXX bind the initializer to u.name in the current basic block and mark it as constant

        visitMemberExpression: (n) ->
                @createPropertyLoad (@visit n.object), n.property, n.computed

        storeValueInDest: (rhvalue, lhs) ->
                if lhs.type is syntax.Identifier
                        dest = @findIdentifierInScope lhs.name, @current_scope
                        if dest?
                                result = llvm.IRBuilder.createStore rhvalue, dest
                        else
                                result = @createPropertyStore @loadGlobal(), lhs, rhvalue, false
                        result
                else if lhs.type is syntax.MemberExpression
                        return @createPropertyStore (@visit lhs.object), lhs.property, rhvalue, lhs.computed
                else
                        throw "unhandled lhs type #{lhs.type}"

        visitAssignmentExpression: (n) ->
                lhs = n.left
                rhs = n.right

                rhvalue = @visit rhs
                if n.operator.length is 2
                        # cribbed from visitBinaryExpression
                        builtin = "binop#{n.operator[0]}"
                        callee = @ejs[builtin]
                        if not callee
                                throw "Internal error: unhandled binary operator '#{n.operator}'"
                        rhvalue = llvm.IRBuilder.createCall callee, [(@visit lhs), rhvalue], "result_#{builtin}"
                        
                @storeValueInDest rhvalue, lhs

                # we need to visit lhs after the store so that we load the value, but only if it's used
                if not n.result_not_used
                        @visit lhs

        visitFunction: (n) ->
                console.warn "        function #{n.ir_name} at line #{n.loc?.start.line}"
                        
                # save off the insert point so we can get back to it after generating this function
                insertBlock = llvm.IRBuilder.getInsertBlock()

                for param in n.params
                        debug.log param.type
                        if param.type is syntax.MemberExpression
                                debug.log param.object.type
                                debug.log param.property.name
                        if param.type isnt syntax.Identifier
                                debug.log "we don't handle destructured/defaulted parameters yet"
                                console.warn JSON.stringify param
                                throw "we don't handle destructured/defaulted parameters yet"

                # XXX this methods needs to be augmented so that we can pass actual types (or the builtin args need
                # to be reflected in jsllvm.cpp too).  maybe we can pass the names to this method and it can do it all
                # there?

                ir_func = n.ir_func
                ir_args = n.ir_func.args
                debug.log ""
                debug.log "ir_func = #{ir_func}"

                debug.log "param #{param.llvm_type} #{param.name}" for param in n.params

                @currentFunction = ir_func

                # Create a new basic block to start insertion into.
                entry_bb = new llvm.BasicBlock "entry", ir_func
                llvm.IRBuilder.setInsertPoint entry_bb

                new_scope = {}

                # we save off the top scope and entry_bb of the function so that we can hoist vars there
                ir_func.topScope = new_scope
                ir_func.entry_bb = entry_bb


                allocas = []

                # create allocas for the builtin args
                for i in [0...BUILTIN_PARAMS.length]
                        alloca = llvm.IRBuilder.createAlloca BUILTIN_PARAMS[i].llvm_type, "local_#{n.params[i].name}"
                        new_scope[n.params[i].name] = alloca
                        allocas.push alloca

                # create an alloca to store our 'EJSValue** args' parameter, so we can pull the formal parameters out of it
                args_alloca = llvm.IRBuilder.createAlloca EjsValueType.pointerTo(), "local_%args"
                new_scope["%args"] = args_alloca
                allocas.push args_alloca

                # now create allocas for the formal parameters
                for param in n.params[BUILTIN_PARAMS.length..]
                        if param.type is syntax.Identifier
                                alloca = llvm.IRBuilder.createAlloca EjsValueType, "local_#{param.name}"
                                new_scope[param.name] = alloca
                                allocas.push alloca
                        else
                                debug.log "we don't handle destructured args at the moment."
                                console.warn JSON.stringify param
                                throw "we don't handle destructured args at the moment."

                debug.log "alloca #{alloca}" for alloca in allocas
                
                # now store the arguments (use .. to include our args array) onto the stack
                for i in [0..BUILTIN_PARAMS.length]
                        store = llvm.IRBuilder.createStore ir_args[i], allocas[i]
                        debug.log "store #{store} *builtin"

                # initialize all our named parameters to undefined
                args_load = llvm.IRBuilder.createLoad args_alloca, "args_load"
                if n.params.length > BUILTIN_PARAMS.length
                        for i in [BUILTIN_PARAMS.length...n.params.length]
                                store = llvm.IRBuilder.createStore @loadUndefinedEjsValue(), allocas[i+1]
                                
                body_bb = new llvm.BasicBlock "body", ir_func
                llvm.IRBuilder.setInsertPoint body_bb

                insertFunc = body_bb.parent
                
                # now pull the named parameters from our args array for the ones that were passed in.
                # any arg that isn't specified
                if n.params.length > BUILTIN_PARAMS.length
                        load_argc = llvm.IRBuilder.createLoad allocas[2], "argc" # FIXME, magic number alert
                        
                        for i in [BUILTIN_PARAMS.length...n.params.length]
                                then_bb  = new llvm.BasicBlock "arg_then", insertFunc
                                else_bb  = new llvm.BasicBlock "arg_else", insertFunc
                                merge_bb = new llvm.BasicBlock "arg_merge", insertFunc

                                cmp = llvm.IRBuilder.createICmpSGt load_argc, (llvm.Constant.getIntegerValue int32Type, i-BUILTIN_PARAMS.length), "argcmpresult"
                                llvm.IRBuilder.createCondBr cmp, then_bb, else_bb
                                
                                llvm.IRBuilder.setInsertPoint then_bb
                                arg_ptr = llvm.IRBuilder.createGetElementPointer args_load, (llvm.Constant.getIntegerValue int32Type, i-BUILTIN_PARAMS.length), "arg#{i-BUILTIN_PARAMS.length}_ptr"
                                debug.log "arg_ptr = #{arg_ptr}"
                                arg = llvm.IRBuilder.createLoad arg_ptr, "arg#{i-BUILTIN_PARAMS.length-1}_load"
                                store = llvm.IRBuilder.createStore arg, allocas[i+1]
                                debug.log "store #{store}"
                                llvm.IRBuilder.createBr merge_bb

                                llvm.IRBuilder.setInsertPoint else_bb
                                llvm.IRBuilder.createBr merge_bb

                                llvm.IRBuilder.setInsertPoint merge_bb
                                
                # stacks of destinations used by continue and break, of the form [{label:..., dest:...}].
                # 
                #  when we see an unlabeled "break" or "continue" we insert a branch to the top bb
                #  when we see a labeled "break" or "continue" we search the stack for the label and branch to that bb
                @continueStack = []
                @breakStack = []
                @iifeStack = []
                
                @visitWithScope new_scope, [n.body]

                # XXX more needed here - this lacks all sorts of control flow stuff.
                # Finish off the function.
                llvm.IRBuilder.createRet @loadUndefinedEjsValue()

                # insert an unconditional branch from entry_bb to body here, now that we're
                # sure we're not going to be inserting allocas into the entry_bb anymore.
                llvm.IRBuilder.setInsertPoint entry_bb
                llvm.IRBuilder.createBr body_bb

                @currentFunction = null

                llvm.IRBuilder.setInsertPoint insertBlock

                console.log "ir_func = #{ir_func}"
                                
                return ir_func

        visitUnaryExpression: (n) ->
                debug.log "operator = '#{n.operator}'"

                builtin = "unop#{n.operator}"
                callee = @ejs[builtin]
                
                if n.operator is "delete"
                        if n.argument.type is syntax.MemberExpression
                                return llvm.IRBuilder.createCall callee, [(@visitOrNull n.argument.object), (@visitOrNull n.argument.property)], "result"
                        return
                else
                        if not callee
                                throw "Internal error: unary operator '#{n.operator}' not implemented"
                        return llvm.IRBuilder.createCall callee, [@visitOrNull n.argument], "result"
                

        visitSequenceExpression: (n) ->
                rv = null
                for exp in n.expressions
                        rv = @visit exp
                rv
                
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
                # inside the else branch, left was truthy
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
                # inside the then branch, left was falsy
                if n.operator is "||"
                        # for || we evaluate the second and store it
                        llvm.IRBuilder.createStore (@visit n.right), result
                else if n.operator is "&&"
                        # for && we short circuit out here
                        llvm.IRBuilder.createStore left_visited, result
                else
                        throw "Internal error 99.1"
                llvm.IRBuilder.createBr merge_bb

                llvm.IRBuilder.setInsertPoint merge_bb
                rv = llvm.IRBuilder.createLoad result, "result_#{n.operator}_load"

                llvm.IRBuilder.setInsertPoint merge_bb

                rv

        visitArgs: (callee, args) ->
                argv = []

                debug.log "args!!!!!!!!!!!!!!!!!!! #{args.length} of them"

                args_offset = 0
                if callee.takes_builtins
                        argv.push @visitOrNull args[0]                                      # % env
                        argv.push null                                                      # %this
                        argv.push llvm.Constant.getIntegerValue int32Type, args.length-1    # argc. subtract 1 for our use of args[0] above
                        args_offset = 1 # we used args[0] already, so skip it in the loop below

                debug.log "args_offset = #{args_offset}"
                if args.length > args_offset
                        argv.push @visitOrNull args[i] for i in [args_offset...args.length]

                argv
                                
        visitCallExpression: (n) ->
                debug.log "visitCall #{JSON.stringify n}"
                debug.log "          arguments length = #{n.arguments.length}"
                debug.log "          arguments[#{i}] =  #{JSON.stringify n.arguments[i]}" for i in [0...n.arguments.length]

                args = n.arguments

                obj = @loadUndefinedEjsValue()
                
                if n.callee.type is syntax.Identifier and n.callee.name[0] == '%'
                        debug.log "builtin"
                        callee = @builtins[n.callee.name.slice(1)]
                        if callee.length # replace with a better Array test
                                callee = callee[n.arguments.length]
                        if n.callee.name is "%invokeClosure"
                                # hack to pull out the proper "this" if the closure is a member expression
                                closure = args[0]
                                if closure.type is syntax.MemberExpression
                                        obj = @visit closure.object
                else if n.callee.type is syntax.MemberExpression
                        debug.log "creating property load!"
                        obj = @visit n.callee.object
                        callee = @createPropertyLoad obj, n.callee.property, n.callee.computed
                else
                        callee = @visit n.callee

                if not callee
                        console.warn "visitCall #{JSON.stringify n}"
                        throw "Internal error: callee should not be null in visitCallExpression"

                # At this point we assume callee is a function object
                argv = @visitArgs callee, args

                if callee.takes_builtins
                        argv[1] = obj

                debug.log "done visiting args"

                # we're dealing with a function here
                # if callee.argSize isnt args.length
                # this isn't invalid in JS.  if argSize > args.length, the args are undefined.
                # if argSize < args.length, the args are still passed

                llvm.IRBuilder.createCall callee, argv, if callee.returnType and callee.returnType.isVoid() then "" else "calltmp"
                
        visitNewExpression: (n) ->
                args = n.arguments

                if n.callee.type is syntax.Identifier and n.callee.name[0] == '%'
                        ctor = @builtins[n.callee.name.slice(1)]
                        if ctor.length  # replace with a better Array test
                                ctor = ctor[n.arguments.length]
                else if n.callee.type is syntax.MemberExpression
                        debug.log "creating property load!"
                        ctor = @createPropertyLoad (@visit n.callee.object), n.callee.property, n.callee.computed
                else
                        ctor = @visit n.callee

                if not ctor
                        throw "Internal error: ctor should not be null"
                

                # ctor isn't the constructor here, by virtue of closure conversion.

                argv = @visitArgs ctor, args

                proto = @createPropertyLoad argv[0], { name: "prototype" }, false
                
                obj = llvm.IRBuilder.createCall @ejs.object_new, [proto], "objtmp"
                if ctor.takes_builtins
                        argv[1] = obj

                llvm.IRBuilder.createCall ctor, argv, "newtmp"
                obj

        visitPropertyAccess: (n) ->
                debug.log "property access: #{nc[1].value}" #NC-USAGE
                throw "whu"

        visitThisExpression: (n) ->
                debug.log "visitThisExpression"
                @createLoadThis()

        visitIdentifier: (n) ->
                debug.log "identifier #{n.name}"
                val = n.name
                source = @findIdentifierInScope val, @current_scope
                if source?
                        rv = llvm.IRBuilder.createLoad source, "load_#{val}"
                        return rv

                # special handling of the arguments object here, so we
                # only initialize/create it if the function is
                # actually going to use it.
                if val is "arguments"
                        arguments_alloca = @createAlloca @currentFunction, EjsValueType, "local_arguments_object"
                        saved_insert_point = llvm.IRBuilder.getInsertBlock()
                        llvm.IRBuilder.setInsertPoint @currentFunction.entry_bb
                        
                        load_argc = llvm.IRBuilder.createLoad @currentFunction.topScope["%argc"], "argc_load"
                        load_args = llvm.IRBuilder.createLoad @currentFunction.topScope["%args"], "args_load"

                        arguments_object = llvm.IRBuilder.createCall @ejs.arguments_new, [load_argc, load_args], "argstmp"
                        llvm.IRBuilder.createStore arguments_object, arguments_alloca
                        @currentFunction.topScope["arguments"] = arguments_alloca

                        llvm.IRBuilder.setInsertPoint saved_insert_point
                        rv = llvm.IRBuilder.createLoad arguments_alloca, "load_arguments"
                        return rv
                        
                        
                rv = null
                debug.log "calling getFunction for #{val}"
                rv = @module.getFunction val

                if not rv
                        debug.log "Symbol '#{val}' not found in current scope"
                        rv = @createPropertyLoad @loadGlobal(), n, false

                debug.log "returning #{rv}"
                rv

        visitObjectExpression: (n) ->
                obj = llvm.IRBuilder.createCall @ejs.object_new, [@loadNullEjsValue()], "objtmp"
                for property in n.properties
                        val = @visit property.value
                        key = property.key
                        @createPropertyStore obj, key, val, false
                return obj

        visitArrayExpression: (n) ->
                obj = llvm.IRBuilder.createCall @ejs.array_new, [(llvm.Constant.getIntegerValue int32Type, n.elements.length)], "arrtmp"
                i = 0;
                for el in n.elements
                        val = @visit el
                        index = type: syntax.Literal, value: i
                        @createPropertyStore obj, index, val, true
                        i = i + 1
                obj
                
        visitExpressionStatement: (n) ->
                n.expression.result_not_used = true
                @visit n.expression

        visitLiteral: (n) ->
                if n.value is null
                        debug.log "literal: null"
                        return @loadNullEjsValue() # this isn't properly typed...  dunno what to do about this here
                else if typeof n.raw is "string" and (n.raw[0] is '"' or n.raw[0] is "'")
                        debug.log "literal string: #{n.value}"
                        c = llvm.IRBuilder.createGlobalStringPtr n.value, "strconst"
                        strcall = llvm.IRBuilder.createCall @ejs.string_new_utf8, [c], "strtmp"
                        debug.log "strcall = #{strcall}"
                        return strcall
                else if typeof n.raw is "string" and n.raw[0] is '/'
                        debug.log "literal regexp: #{n.raw}"
                        c = llvm.IRBuilder.createGlobalStringPtr n.raw, "strconst"
                        regexpcall = llvm.IRBuilder.createCall @ejs.regexp_new_utf8, [c], "regexptmp"
                        debug.log "regexpcall = #{strcall}"
                        return regexpcall
                else if typeof n.value is "number"
                        debug.log "literal number: #{n.value}"
                        c = llvm.ConstantFP.getDouble n.value
                        return llvm.IRBuilder.createCall @ejs.number_new, [c], "numtmp"
                else if typeof n.value is "boolean"
                        debug.log "literal boolean: #{n.value}"
                        c = llvm.Constant.getIntegerValue boolType, (if n.value then 1 else 0)
                        return llvm.IRBuilder.createCall @ejs.boolean_new, [c], "booltmp"
                console.warn "n.raw[0] = #{n.raw[0]}"
                throw "Internal error: unrecognized literal of type #{typeof n.value}"

class AddFunctionsVisitor extends NodeVisitor
        constructor: (@module) ->
                super

        visitFunction: (n) ->
                n.ir_name = "_ejs_anonymous"
                if n?.id?.name?
                        n.ir_name = n.id.name

                # at this point point n.params includes %env as its first param, and is followed by all the formal parameters from the original
                # script source.  we insert %this and %argc between these.  the LLVMIR phase later removes the actual formal parameters and
                # adds the "EJSValue** args" array, loading the formal parameter values from that.

                n.params[0].llvm_type = BUILTIN_PARAMS[0].llvm_type
                n.params.splice 1, 0, BUILTIN_PARAMS[1]
                n.params.splice 2, 0, BUILTIN_PARAMS[2]

                # set the types of all later arguments to be EjsValueType
                param.llvm_type = EjsValueType for param in n.params[BUILTIN_PARAMS.length..]

                # the LLVMIR func we allocate takes the proper EJSValue** parameter in the 4th spot instead of all the parameters
                n.ir_func = takes_builtins @module.getOrInsertFunction n.ir_name, EjsValueType, (param.llvm_type for param in BUILTIN_PARAMS).concat [EjsValueType.pointerTo()]
                
                ir_args = n.ir_func.args
                (ir_args[i].setName n.params[i].name) for i in [0...BUILTIN_PARAMS.length]
                ir_args[BUILTIN_PARAMS.length].setName "%args"

                # we don't need to recurse here since we won't have nested functions at this point
                n
                        
insert_toplevel_func = (tree, filename) ->
        sanitize = (filename) ->
                filename = filename.replace /\.js$/, ""
                filename = filename.replace /[.,-\/\\]/g, "_" # this is insanely inadequate
                filename
        
        toplevel =
                type: syntax.FunctionDeclaration,
                id:
                        type: syntax.Identifier
                        name: "_ejs_toplevel_#{sanitize filename}"
                params: [
                        { type: syntax.Identifier, name: "%env_unused" }
                ]
                body:
                        type: syntax.BlockStatement
                        body: tree.body
                toplevel: true
        tree.body = [toplevel]
        tree

class MoveFunctionDeclsToStartOfBlock extends NodeVisitor
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
                


exports.compile = (tree, filename) ->

        console.warn "compiling #{filename}"
        
        tree = insert_toplevel_func tree, filename

        visitor = new MoveFunctionDeclsToStartOfBlock
        tree = visitor.visit tree
        debug.log -> escodegen.generate tree

        toplevel_name = tree.body[0].id.name
        
        #tree = desugar tree
        #debug.log -> escodegen.generate tree
        
        tree = closure_conversion.convert tree

        debug.log "after closure conversion"
        debug.log -> escodegen.generate tree

        module = new llvm.Module "compiled-#{filename}"

        module.toplevel_name = toplevel_name

        visitor = new AddFunctionsVisitor module
        tree = visitor.visit tree

        debug.log -> escodegen.generate tree

        visitor = new LLVMIRVisitor module
        visitor.visit tree

        module
