esprima = require 'esprima'
escodegen = require 'escodegen'
syntax = esprima.Syntax
debug = require 'debug'
path = require 'path'

{ Set } = require 'set'
{ NodeVisitor } = require 'nodevisitor'
closure_conversion = require 'closure-conversion'
{ genId } = require 'echo-util'

{ ExitableScope, TryExitableScope, SwitchExitableScope, LoopExitableScope } = require 'exitable-scope'

types = require 'types'

llvm = require 'llvm'
ir = llvm.IRBuilder

# set to true to inline more of the call sequence at call sites (we still have to call into the runtime to decompose the closure itself for now)
# disable this for now because it breaks more of the exception tests
decompose_closure_on_invoke = false

# special key for parent scope when performing lookups
PARENT_SCOPE_KEY = ":parent:"

BUILTIN_PARAMS = [
  { type: syntax.Identifier, name: "%closure", llvm_type: types.EjsClosureEnv }
  { type: syntax.Identifier, name: "%this",    llvm_type: types.EjsValue }
  { type: syntax.Identifier, name: "%argc",    llvm_type: types.int32 }
]

jscharConst = (c) ->
        constant = llvm.Constant.getIntegerValue types.jschar, c
        constant.is_constant = true
        constant.constant_val = c
        constant

int32Const = (c) ->
        constant = llvm.Constant.getIntegerValue types.int32, c
        constant.is_constant = true
        constant.constant_val = c
        constant
                
int64Const = (c) ->
        constant = llvm.Constant.getIntegerValue types.int64, c
        constant.is_constant = true
        constant.constant_val = c
        constant

nullConst = (t) -> llvm.Constant.getNull t

trueConst = -> llvm.Constant.getIntegerValue types.bool, 1
falseConst = -> llvm.Constant.getIntegerValue types.bool, 0

boolConst = (c) ->
        constant = llvm.Constant.getIntegerValue types.bool, if c is false then 0 else 1
        constant.is_constant = true
        constant.constant_val = c
        constant

takes_builtins = (n) ->
        n.takes_builtins = true
        n

only_reads_memory = (n) ->
        n.setOnlyReadsMemory()
        n

does_not_access_memory = (n) ->
        n.setDoesNotAccessMemory()
        n

does_not_throw = (n) ->
        n.setDoesNotThrow()
        n

hasOwn = Object::hasOwnProperty

class LLVMIRVisitor extends NodeVisitor
        constructor: (@module, @filename) ->

                # build up our runtime method table
                @ejs_intrinsics = {
                        invokeClosure: @handleInvokeClosureIntrinsic
                        makeClosure: @handleMakeClosureIntrinsic
                        makeAnonClosure: @handleMakeAnonClosureIntrinsic
                        createArgScratchArea: @handleCreateArgScratchAreaIntrinsic
                }

                @llvm_intrinsics = {
                        gcroot: module.getOrInsertIntrinsic "@llvm.gcroot"
                }
                
                @ejs_runtime = {
                        personality:           module.getOrInsertExternalFunction "__ejs_personality_v0",           types.int32, [types.int32, types.int32, types.int64, types.int8Pointer, types.int8Pointer]

                        invoke_closure:        takes_builtins module.getOrInsertExternalFunction "_ejs_invoke_closure", types.EjsValue, [types.EjsValue, types.EjsValue, types.int32, types.EjsValue.pointerTo()]
                        make_closure:          module.getOrInsertExternalFunction "_ejs_function_new", types.EjsValue, [types.EjsClosureEnv, types.EjsValue, types.EjsClosureFunc]
                        make_anon_closure:     module.getOrInsertExternalFunction "_ejs_function_new_anon", types.EjsValue, [types.EjsClosureEnv, types.EjsClosureFunc]
                        decompose_closure:     module.getOrInsertExternalFunction "_ejs_decompose_closure", types.bool, [types.EjsValue, types.EjsClosureFunc.pointerTo(), types.EjsClosureEnv.pointerTo(), types.EjsValue.pointerTo()]
                                                
                        object_create:         module.getOrInsertExternalFunction "_ejs_object_create",             types.EjsValue, [types.EjsValue]
                        arguments_new:         module.getOrInsertExternalFunction "_ejs_arguments_new",             types.EjsValue, [types.int32, types.EjsValue.pointerTo()]
                        array_new:             module.getOrInsertExternalFunction "_ejs_array_new",                 types.EjsValue, [types.int32]
                        number_new:            does_not_throw does_not_access_memory module.getOrInsertExternalFunction "_ejs_number_new",                types.EjsValue, [types.double]
                        string_new_utf8:       only_reads_memory does_not_throw module.getOrInsertExternalFunction "_ejs_string_new_utf8",           types.EjsValue, [types.string]
                        regexp_new_utf8:       module.getOrInsertExternalFunction "_ejs_regexp_new_utf8",           types.EjsValue, [types.string]
                        truthy:                does_not_throw does_not_access_memory module.getOrInsertExternalFunction "_ejs_truthy",                    types.bool, [types.EjsValue]
                        object_setprop:        module.getOrInsertExternalFunction "_ejs_object_setprop",            types.EjsValue, [types.EjsValue, types.EjsValue, types.EjsValue]
                        object_getprop:        only_reads_memory module.getOrInsertExternalFunction "_ejs_object_getprop",           types.EjsValue, [types.EjsValue, types.EjsValue]
                        object_getprop_utf8:   only_reads_memory module.getOrInsertExternalFunction "_ejs_object_getprop_utf8",      types.EjsValue, [types.EjsValue, types.string]
                        object_setprop_utf8:   module.getOrInsertExternalFunction "_ejs_object_setprop_utf8",       types.EjsValue, [types.EjsValue, types.string, types.EjsValue]
                        prop_iterator_new:     module.getOrInsertExternalFunction "_ejs_property_iterator_new",     types.EjsPropIterator, [types.EjsValue]
                        prop_iterator_current: module.getOrInsertExternalFunction "_ejs_property_iterator_current", types.EjsValue, [types.EjsPropIterator]
                        prop_iterator_next:    module.getOrInsertExternalFunction "_ejs_property_iterator_next",    types.bool, [types.EjsPropIterator, types.bool]
                        prop_iterator_free:    module.getOrInsertExternalFunction "_ejs_property_iterator_free",    types.void, [types.EjsPropIterator]
                        begin_catch:           module.getOrInsertExternalFunction "_ejs_begin_catch",               types.EjsValue, [types.int8Pointer]
                        end_catch:             module.getOrInsertExternalFunction "_ejs_end_catch",                 types.EjsValue, []
                        throw:                 module.getOrInsertExternalFunction "_ejs_throw",                     types.void, [types.EjsValue]
                        rethrow:               module.getOrInsertExternalFunction "_ejs_rethrow",                   types.void, [types.EjsValue]

                        init_string_literal:   module.getOrInsertExternalFunction "_ejs_string_init_literal",       types.void, [types.string, types.EjsValue.pointerTo(), types.EjsPrimString.pointerTo(), types.jschar.pointerTo(), types.int32]
                        
                        undefined:             module.getOrInsertGlobal           "_ejs_undefined",                 types.EjsValue
                        "true":                module.getOrInsertGlobal           "_ejs_true",                      types.EjsValue
                        "false":               module.getOrInsertGlobal           "_ejs_false",                     types.EjsValue
                        "null":                module.getOrInsertGlobal           "_ejs_null",                      types.EjsValue
                        "one":                 module.getOrInsertGlobal           "_ejs_one",                       types.EjsValue
                        "zero":                module.getOrInsertGlobal           "_ejs_zero",                      types.EjsValue
                        "atom-null":           module.getOrInsertGlobal           "_ejs_atom_null",                 types.EjsValue
                        "atom-undefined":      module.getOrInsertGlobal           "_ejs_atom_undefined",            types.EjsValue
                        "atom-length":         module.getOrInsertGlobal           "_ejs_atom_length",               types.EjsValue
                        "atom-__ejs":          module.getOrInsertGlobal           "_ejs_atom___ejs",                types.EjsValue
                        "atom-object":         module.getOrInsertGlobal           "_ejs_atom_object",               types.EjsValue
                        "atom-function":       module.getOrInsertGlobal           "_ejs_atom_function",             types.EjsValue
                        "atom-prototype":      module.getOrInsertGlobal           "_ejs_atom_prototype",            types.EjsValue
                        "atom-Object":         module.getOrInsertGlobal           "_ejs_atom_Object",               types.EjsValue
                        "atom-Array":          module.getOrInsertGlobal           "_ejs_atom_Array",                types.EjsValue
                        global:                module.getOrInsertGlobal           "_ejs_global",                    types.EjsValue
                        exception_typeinfo:    module.getOrInsertGlobal           "EJS_EHTYPE_ejsvalue",            types.EjsExceptionTypeInfo

                        "unop-":           module.getOrInsertExternalFunction "_ejs_op_neg",         types.EjsValue, [types.EjsValue]
                        "unop+":           module.getOrInsertExternalFunction "_ejs_op_plus",        types.EjsValue, [types.EjsValue]
                        "unop!":           module.getOrInsertExternalFunction "_ejs_op_not",         types.EjsValue, [types.EjsValue]
                        "unop~":           module.getOrInsertExternalFunction "_ejs_op_bitwise_not", types.EjsValue, [types.EjsValue]
                        "unoptypeof":      does_not_throw module.getOrInsertExternalFunction "_ejs_op_typeof",      types.EjsValue, [types.EjsValue]
                        "unopdelete":      module.getOrInsertExternalFunction "_ejs_op_delete",      types.EjsValue, [types.EjsValue, types.EjsValue] # this is a unop, but ours only works for memberexpressions
                        "unopvoid":        module.getOrInsertExternalFunction "_ejs_op_void",        types.EjsValue, [types.EjsValue]
                        "binop^":          module.getOrInsertExternalFunction "_ejs_op_bitwise_xor", types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop&":          module.getOrInsertExternalFunction "_ejs_op_bitwise_and", types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop|":          module.getOrInsertExternalFunction "_ejs_op_bitwise_or",  types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop>>":         module.getOrInsertExternalFunction "_ejs_op_rsh",         types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop<<":         module.getOrInsertExternalFunction "_ejs_op_lsh",         types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop>>>":        module.getOrInsertExternalFunction "_ejs_op_ursh",        types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop<<<":        module.getOrInsertExternalFunction "_ejs_op_ulsh",        types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop%":          module.getOrInsertExternalFunction "_ejs_op_mod",         types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop+":          module.getOrInsertExternalFunction "_ejs_op_add",         types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop*":          module.getOrInsertExternalFunction "_ejs_op_mult",        types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop/":          module.getOrInsertExternalFunction "_ejs_op_div",         types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop<":          module.getOrInsertExternalFunction "_ejs_op_lt",          types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop<=":         module.getOrInsertExternalFunction "_ejs_op_le",          types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop>":          module.getOrInsertExternalFunction "_ejs_op_gt",          types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop>=":         module.getOrInsertExternalFunction "_ejs_op_ge",          types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop-":          module.getOrInsertExternalFunction "_ejs_op_sub",         types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop===":        does_not_throw does_not_access_memory module.getOrInsertExternalFunction "_ejs_op_strict_eq",   types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop==":         module.getOrInsertExternalFunction "_ejs_op_eq",          types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop!==":        does_not_throw does_not_access_memory module.getOrInsertExternalFunction "_ejs_op_strict_neq",  types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binop!=":         module.getOrInsertExternalFunction "_ejs_op_neq",         types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binopinstanceof": module.getOrInsertExternalFunction "_ejs_op_instanceof",  types.EjsValue, [types.EjsValue, types.EjsValue]
                        "binopin":         module.getOrInsertExternalFunction "_ejs_op_in",          types.EjsValue, [types.EjsValue, types.EjsValue]
                }

                @module_atoms = Object.create null
                @initGlobalScope()
                
                @literalInitializationFunction = @module.getOrInsertFunction "_ejs_module_init_string_literals_#{@filename}", types.void, []
                entry_bb = new llvm.BasicBlock "entry", @literalInitializationFunction
                return_bb = new llvm.BasicBlock "return", @literalInitializationFunction

                ir.setInsertPoint entry_bb
                ir.createBr return_bb
                        
                ir.setInsertPoint return_bb
                ir.createRetVoid()

                @literalInitializationBB = entry_bb

        # lots of helper methods

        createLoad: (value, name) ->
                rv = ir.createLoad value, name
                rv
                
        loadBoolEjsValue: (n) ->
                boolval = @createLoad (if n then @ejs_runtime['true'] else @ejs_runtime['false']), "load_bool"
                boolval.is_constant = true
                boolval.constant_val = n
                boolval
                
        loadNullEjsValue: ->
                nullval = @createLoad @ejs_runtime['null'], "load_null"
                nullval.is_constant = true
                nullval.constant_val = null
                nullval
                
        loadUndefinedEjsValue: ->
                undef = @createLoad @ejs_runtime.undefined, "load_undefined"
                undef.is_constant = true
                undef.constant_val = undefined
                undef
                
        loadGlobal: -> @createLoad @ejs_runtime.global, "load_global"

        pushIIFEInfo: (info) ->
                @iifeStack.unshift info
                
        popIIFEInfo: ->
                @iifeStack.shift()

        initGlobalScope: ->
                @current_scope = Object.create null

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
                        if hasOwn.call scope, ident
                                return scope[ident]
                        scope = scope[PARENT_SCOPE_KEY]
                null

                                
        createAlloca: (func, type, name) ->
                saved_insert_point = ir.getInsertBlock()
                ir.setInsertPointStartBB func.entry_bb
                alloca = ir.createAlloca type, name

                # if EjsValue was a pointer value we would be able to use an the llvm gcroot intrinsic here.  but with the nan boxing
                # we kinda lose out as the llvm IR code doesn't permit non-reference types to be gc roots.
                # if type is types.EjsValue
                #        # EjsValues are rooted
                #        @createCall @llvm_intrinsics.gcroot, [(ir.createPointerCast alloca, types.int8Pointer.pointerTo(), "rooted_alloca"), nullConst types.int8Pointer], ""

                ir.setInsertPoint saved_insert_point
                alloca

        createAllocas: (func, ids, scope) ->
                allocas = []
                new_allocas = []
                
                # the allocas are always allocated in the function entry_bb so the mem2reg opt pass can regenerate the ssa form for us
                saved_insert_point = ir.getInsertBlock()
                ir.setInsertPointStartBB func.entry_bb

                j = 0
                for i in [0...ids.length]
                        name = ids[i].id.name
                        if !hasOwn.call scope, name
                                allocas[j] = ir.createAlloca types.EjsValue, "local_#{name}"
                                scope[name] = allocas[j]
                                new_allocas[j] = true
                        else
                                allocas[j] = scope[name]
                                new_allocas[j] = false
                        j = j + 1
                                

                # reinstate the IRBuilder to its previous insert point so we can insert the actual initializations
                ir.setInsertPoint saved_insert_point

                { allocas: allocas, new_allocas: new_allocas }

        createPropertyStore: (obj,prop,rhs,computed) ->
                if computed
                        # we store obj[prop], prop can be any value
                        prop_alloca = @createAlloca @currentFunction, types.EjsValue, "prop_alloca"
                        ir.createStore (@visit prop), prop_alloca
                        @createCall @ejs_runtime.object_setprop, [obj, (@createLoad prop_alloca, "%prop_alloca"), rhs], "propstore_computed"
                else
                        # we store obj.prop, prop is an id
                        if prop.type is syntax.Identifier
                                pname = prop.name
                        else # prop.type is syntax.Literal
                                pname = prop.value

                        c = @getAtom pname

                        debug.log -> "createPropertyStore #{obj}[#{pname}]"
                        
                        @createCall @ejs_runtime.object_setprop, [obj, c, rhs], "propstore_#{pname}"
                
        createPropertyLoad: (obj,prop,computed,canThrow = true) ->
                if computed
                        # we load obj[prop], prop can be any value
                        loadprop = @visit prop
                        pname = "computed"
                        @createCall @ejs_runtime.object_getprop, [obj, loadprop], "getprop_#{pname}"
                else
                        # we load obj.prop, prop is an id
                        pname = @getAtom prop.name
                        @createCall @ejs_runtime.object_getprop, [obj, pname], "getprop_#{prop.name}", canThrow
                

        createLoadThis: () ->
                _this = @findIdentifierInScope "%this", @current_scope
                return @createLoad _this, "load_this"


        visitOrNull: (n) -> (@visit n) || @loadNullEjsValue()
        visitOrUndefined: (n) -> (@visit n) || @loadUndefinedEjsValue()
        
        visitProgram: (n) ->
                # by the time we make it here the program has been
                # transformed so that there is nothing at the toplevel
                # but function declarations.
                @visit func for func in n.body

        visitBlock: (n) ->
                new_scope = Object.create null

                iife_rv = null
                iife_bb = null
                
                if n.fromIIFE
                        insertBlock = ir.getInsertBlock()
                        insertFunc = insertBlock.parent
                        
                        iife_rv = @createAlloca @currentFunction, types.EjsValue, "%iife_rv"
                        iife_bb = new llvm.BasicBlock "iife_dest", insertFunc

                @pushIIFEInfo iife_rv: iife_rv, iife_dest_bb: iife_bb

                @visitWithScope new_scope, n.body

                @popIIFEInfo()
                if iife_bb
                        ir.createBr iife_bb
                        ir.setInsertPoint iife_bb
                        rv = @createLoad iife_rv, "%iife_rv_load"
                        rv
                else
                        n

        visitSwitch: (n) ->
                insertBlock = ir.getInsertBlock()
                insertFunc = insertBlock.parent

                switch_bb = new llvm.BasicBlock "switch", insertFunc

                ir.createBr switch_bb
                ir.setInsertPoint switch_bb
                
                # find the default: case first
                defaultCase = null
                (if not _case.test then defaultCase = _case) for _case in n.cases

                # for each case, create 2 basic blocks
                for _case in n.cases
                        if _case isnt defaultCase
                                _case.dest_check = new llvm.BasicBlock "case_dest_check_bb", insertFunc

                for _case in n.cases
                        _case.bb = new llvm.BasicBlock "case_bb", insertFunc

                merge_bb = new llvm.BasicBlock "switch_merge", insertFunc

                discr = @visit n.discriminant

                case_checks = []
                for _case in n.cases
                        if defaultCase isnt _case
                                case_checks.push test: _case.test, dest_check: _case.dest_check, body: _case.bb

                case_checks.push dest_check: if defaultCase? then defaultCase.bb else merge_bb

                scope = new SwitchExitableScope merge_bb
                scope.enter()

                # insert all the code for the tests
                ir.createBr case_checks[0].dest_check
                ir.setInsertPoint case_checks[0].dest_check
                for casenum in [0...case_checks.length-1]
                        test = @visit case_checks[casenum].test
                        discTest = @createCall @ejs_runtime["binop==="], [discr, test], "test"
                        disc_truthy = @createCall @ejs_runtime.truthy, [discTest], "disc_truthy"
                        disc_cmp = ir.createICmpEq disc_truthy, falseConst(), "disccmpresult"
                        ir.createCondBr disc_cmp, case_checks[casenum+1].dest_check, case_checks[casenum].body
                        ir.setInsertPoint case_checks[casenum+1].dest_check


                case_bodies = []
                
                # now insert all the code for the case consequents
                for _case in n.cases
                        case_bodies.push bb:_case.bb, consequent:_case.consequent

                case_bodies.push bb:merge_bb
                
                for casenum in [0...case_bodies.length-1]
                        ir.setInsertPoint case_bodies[casenum].bb
                        for c of case_bodies[casenum].consequent
                                @visit case_bodies[casenum].consequent[c]
                        ir.createBr case_bodies[casenum+1].bb
                        
                ir.setInsertPoint merge_bb

                scope.leave()

                merge_bb
                
        visitCase: (n) ->
                throw "we shouldn't get here, case statements are handled in visitSwitch"
                        
                
        visitLabeledStatement: (n) ->
                n.body.label = n.label.name
                @visit n.body

        visitBreak: (n) ->
                # unlabeled breaks just exit our enclosing exitable scope.  breaks are valid in
                # switch statements, though, so we can't just use LoopExitableScope.findFirst()
                return TryExitableScope.findFirstNonTry().exitAft true if not n.label
                # for the labeled case, we search up the stack for the right scope and exitAft that one
                scope = LoopExitableScope.findLabeled n.label.name
                # XXX check if scope is null, throw if so
                scope.exitAft true

        visitContinue: (n) ->
                # unlabeled continue just exit our enclosing exitable scope
                return LoopExitableScope.findFirst().exitFore() if not n.label

                # for the labeled case, we search up the stack for the right scope and exitFore that one
                scope = LoopExitableScope.findLabeled n.label.name
                # XXX check if scope is null, throw if so
                scope.exitFore()
                
        visitFor: (n) ->
                insertBlock = ir.getInsertBlock()
                insertFunc = insertBlock.parent

                init_bb = new llvm.BasicBlock "for_init", insertFunc
                test_bb = new llvm.BasicBlock "for_test", insertFunc
                body_bb = new llvm.BasicBlock "for_body", insertFunc
                update_bb = new llvm.BasicBlock "for_update", insertFunc
                merge_bb = new llvm.BasicBlock "for_merge", insertFunc

                ir.createBr init_bb
                
                ir.setInsertPoint init_bb
                @visit n.init
                ir.createBr test_bb

                ir.setInsertPoint test_bb
                if n.test
                        cond_truthy = @createCall @ejs_runtime.truthy, [@visit(n.test)], "cond_truthy"
                        cmp = ir.createICmpEq cond_truthy, falseConst(), "cmpresult"
                        ir.createCondBr cmp, merge_bb, body_bb
                else
                        ir.createBr body_bb

                scope = new LoopExitableScope n.label, update_bb, merge_bb
                scope.enter()

                ir.setInsertPoint body_bb
                @visit n.body
                ir.createBr update_bb

                ir.setInsertPoint update_bb
                @visit n.update
                ir.createBr test_bb

                scope.leave()

                ir.setInsertPoint merge_bb
                merge_bb
                                
        visitWhile: (n) ->
                insertBlock = ir.getInsertBlock()
                insertFunc = insertBlock.parent
                
                while_bb  = new llvm.BasicBlock "while_start", insertFunc
                body_bb = new llvm.BasicBlock "while_body", insertFunc
                merge_bb = new llvm.BasicBlock "while_merge", insertFunc

                ir.createBr while_bb
                ir.setInsertPoint while_bb
                
                cond_truthy = @createCall @ejs_runtime.truthy, [@visit(n.test)], "cond_truthy"
                cmp = ir.createICmpEq cond_truthy, falseConst(), "cmpresult"
                
                ir.createCondBr cmp, merge_bb, body_bb

                scope = new LoopExitableScope n.label, while_bb, merge_bb
                scope.enter()
                
                ir.setInsertPoint body_bb
                @visit n.body
                ir.createBr while_bb

                scope.leave()
                                
                ir.setInsertPoint merge_bb
                merge_bb

        visitForIn: (n) ->
                insertBlock = ir.getInsertBlock()
                insertFunc = insertBlock.parent

                iterator = @createCall @ejs_runtime.prop_iterator_new, [@visit n.right], "iterator"

                # make sure we get an alloca if there's a "var"
                if n.left[0]?
                        @visit n.left
                        lhs = n.left[0].declarations[0].id
                else
                        lhs = n.left
                
                forin_bb  = new llvm.BasicBlock "forin_start", insertFunc
                body_bb   = new llvm.BasicBlock "forin_body",  insertFunc
                merge_bb  = new llvm.BasicBlock "forin_merge", insertFunc
                                
                ir.createBr forin_bb
                ir.setInsertPoint forin_bb

                moreleft = @createCall @ejs_runtime.prop_iterator_next, [iterator, trueConst()], "moreleft"
                cmp = ir.createICmpEq moreleft, falseConst(), "cmpmoreleft"
                ir.createCondBr cmp, merge_bb, body_bb
                
                ir.setInsertPoint body_bb
                current = @createCall @ejs_runtime.prop_iterator_current, [iterator], "iterator_current"
                @storeValueInDest current, lhs
                @visit n.body
                ir.createBr forin_bb

                ir.setInsertPoint merge_bb
                merge_bb
                
                
        visitUpdateExpression: (n) ->
                result = @createAlloca @currentFunction, types.EjsValue, "%update_result"
                argument = @visit n.argument
                
                one = @createLoad @ejs_runtime['one'], "load_one"
                
                if not n.prefix
                        # postfix updates store the argument before the op
                        ir.createStore argument, result

                # argument = argument $op 1
                temp = @createCall @ejs_runtime["binop#{if n.operator is '++' then '+' else '-'}"], [argument, one], "update_temp"
                
                @storeValueInDest temp, n.argument
                
                # return result
                if n.prefix
                        argument = @visit n.argument
                        # prefix updates store the argument after the op
                        ir.createStore argument, result
                @createLoad result, "%update_result_load"

        visitConditionalExpression: (n) ->
                @visitIfOrCondExp n, true
                        
        visitIf: (n) ->
                @visitIfOrCondExp n, false

        visitIfOrCondExp: (n, load_result) ->

                if load_result
                        cond_val = @createAlloca @currentFunction, types.EjsValue, "%cond_val"
                
                # first we convert our conditional EJSValue to a boolean
                cond_truthy = @createCall @ejs_runtime.truthy, [@visit(n.test)], "cond_truthy"

                insertBlock = ir.getInsertBlock()
                insertFunc = insertBlock.parent

                then_bb  = new llvm.BasicBlock "then", insertFunc
                else_bb  = new llvm.BasicBlock "else", insertFunc
                merge_bb = new llvm.BasicBlock "merge", insertFunc

                # we invert the test here - check if the condition is false/0
                cmp = ir.createICmpEq cond_truthy, falseConst(), "cmpresult"
                ir.createCondBr cmp, else_bb, then_bb

                ir.setInsertPoint then_bb
                then_val = @visit n.consequent
                if load_result
                        ir.createStore then_val, cond_val
                ir.createBr merge_bb

                ir.setInsertPoint else_bb
                else_val = @visit n.alternate
                if load_result
                        ir.createStore else_val, cond_val
                ir.createBr merge_bb

                ir.setInsertPoint merge_bb
                if load_result
                        @createLoad cond_val, "cond_val_load"
                else
                        merge_bb
                
        visitReturn: (n) ->
                debug.log "visitReturn"
                if @iifeStack[0].iife_rv?
                        if n.argument?
                                ir.createStore (@visit n.argument), @iifeStack[0].iife_rv
                        else
                                ir.createStore @loadUndefinedEjsValue(), @iifeStack[0].iife_rv
                        ir.createBr @iifeStack[0].iife_dest_bb
                else
                        rv = if n.argument? then (@visit n.argument) else @loadUndefinedEjsValue()
                        
                        if @finallyStack.length > 0
                                @returnValueAlloca = @createAlloca @currentFunction, types.EjsValue, "returnValue" unless @returnValueAlloca?
                                ir.createStore rv, @returnValueAlloca
                                ir.createStore (int32Const ExitableScope.REASON_RETURN), @currentFunction.cleanup_reason
                                ir.createBr @finallyStack[0]
                        else
                                return_alloca = @createAlloca @currentFunction, types.EjsValue, "return_alloca"
                                ir.createStore rv, return_alloca
                        
                                ir.createRet @createLoad return_alloca, "return_load"
                                                

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
                                                ir.createStore initializer, allocas[i]
                                else
                                        initializer = @visitOrUndefined n.declarations[i].init
                                        ir.createStore initializer, allocas[i]
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
                                                ir.createStore initializer, allocas[i]
                                else
                                        initializer = @visitOrUndefined n.declarations[i].init
                                        ir.createStore initializer, allocas[i]
                else if n.kind is "const"
                        for i in [0...n.declarations.length]
                                u = n.declarations[i]
                                initializer_ir = @visit u.init
                                # XXX bind the initializer to u.name in the current basic block and mark it as constant

        visitMemberExpression: (n) ->
                obj_result = @createAlloca @currentFunction, types.EjsValue, "result_obj"
                obj_visit = @visit n.object
                ir.createStore obj_visit, obj_result
                obj_load = @createLoad obj_result, "obj_load"
                rv = @createPropertyLoad obj_load, n.property, n.computed
                load_result = @createAlloca @currentFunction, types.EjsValue, "load_result"
                ir.createStore rv, load_result
                if not n.result_not_used
                        @createLoad load_result, "rv"

        storeValueInDest: (rhvalue, lhs) ->
                if lhs.type is syntax.Identifier
                        dest = @findIdentifierInScope lhs.name, @current_scope
                        if dest?
                                result = ir.createStore rhvalue, dest
                        else
                                result = @createPropertyStore @loadGlobal(), lhs, rhvalue, false
                        result
                else if lhs.type is syntax.MemberExpression
                        object_alloca = @createAlloca @currentFunction, types.EjsValue, "object_alloca"
                        ir.createStore (@visit lhs.object), object_alloca
                        result = @createPropertyStore (@createLoad object_alloca, "load_object"), lhs.property, rhvalue, lhs.computed
                else
                        throw "unhandled lhs type #{lhs.type}"

        visitAssignmentExpression: (n) ->
                lhs = n.left
                rhs = n.right

                rhvalue = @visit rhs
                if n.operator.length is 2
                        # cribbed from visitBinaryExpression
                        builtin = "binop#{n.operator[0]}"
                        callee = @ejs_runtime[builtin]
                        if not callee
                                throw "Internal error: unhandled binary operator '#{n.operator}'"
                        rhvalue = @createCall callee, [(@visit lhs), rhvalue], "result_#{builtin}"
                
                @storeValueInDest rhvalue, lhs

                # we need to visit lhs after the store so that we load the value, but only if it's used
                if not n.result_not_used
                        @visit lhs

        visitFunction: (n) ->
                if not n.toplevel?
                        console.warn "        function #{n.ir_name} at #{@filename}:#{if n.loc? then n.loc.start.line else '<unknown>'}"
                
                # save off the insert point so we can get back to it after generating this function
                insertBlock = ir.getInsertBlock()

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
                #debug.log -> "ir_func = #{ir_func}"

                #debug.log -> "param #{param.llvm_type} #{param.name}" for param in n.params

                @currentFunction = ir_func

                # Create a new basic block to start insertion into.
                entry_bb = new llvm.BasicBlock "entry", ir_func

                ir.setInsertPoint entry_bb

                new_scope = Object.create null

                # we save off the top scope and entry_bb of the function so that we can hoist vars there
                ir_func.topScope = new_scope
                ir_func.entry_bb = entry_bb

                ir_func.literalAllocas = Object.create null

                allocas = []

                # create allocas for the builtin args
                for i in [0...BUILTIN_PARAMS.length]
                        alloca = ir.createAlloca BUILTIN_PARAMS[i].llvm_type, "local_#{n.params[i].name}"
                        new_scope[n.params[i].name] = alloca
                        allocas.push alloca

                # create an alloca to store our 'EJSValue** args' parameter, so we can pull the formal parameters out of it
                args_alloca = ir.createAlloca types.EjsValue.pointerTo(), "local_%args"
                new_scope["%args"] = args_alloca
                allocas.push args_alloca

                # now create allocas for the formal parameters
                for param in n.params[BUILTIN_PARAMS.length..]
                        if param.type is syntax.Identifier
                                alloca = @createAlloca @currentFunction, types.EjsValue, "local_#{param.name}"
                                new_scope[param.name] = alloca
                                allocas.push alloca
                        else
                                debug.log "we don't handle destructured args at the moment."
                                console.warn JSON.stringify param
                                throw "we don't handle destructured args at the moment."

                debug.log -> "alloca #{alloca}" for alloca in allocas
        
                # now store the arguments (use .. to include our args array) onto the stack
                for i in [0..BUILTIN_PARAMS.length]
                        store = ir.createStore ir_args[i], allocas[i]
                        debug.log -> "store #{store} *builtin"

                # initialize all our named parameters to undefined
                args_load = @createLoad args_alloca, "args_load"
                if n.params.length > BUILTIN_PARAMS.length
                        for i in [BUILTIN_PARAMS.length...n.params.length]
                                store = ir.createStore @loadUndefinedEjsValue(), allocas[i+1]
                        
                body_bb = new llvm.BasicBlock "body", ir_func
                ir.setInsertPoint body_bb

                if n.toplevel?
                        ir.createCall @literalInitializationFunction, [], ""

                insertFunc = body_bb.parent
        
                # now pull the named parameters from our args array for the ones that were passed in.
                # any arg that isn't specified
                if n.params.length > BUILTIN_PARAMS.length
                        load_argc = @createLoad allocas[2], "argc" # FIXME, magic number alert
                
                        for i in [BUILTIN_PARAMS.length...n.params.length]
                                then_bb  = new llvm.BasicBlock "arg_then", insertFunc
                                else_bb  = new llvm.BasicBlock "arg_else", insertFunc
                                merge_bb = new llvm.BasicBlock "arg_merge", insertFunc

                                cmp = ir.createICmpSGt load_argc, (int32Const i-BUILTIN_PARAMS.length), "argcmpresult"
                                ir.createCondBr cmp, then_bb, else_bb
                        
                                ir.setInsertPoint then_bb
                                arg_ptr = ir.createGetElementPointer args_load, [(int32Const i-BUILTIN_PARAMS.length)], "arg#{i-BUILTIN_PARAMS.length}_ptr"
                                debug.log -> "arg_ptr = #{arg_ptr}"
                                arg = @createLoad arg_ptr, "arg#{i-BUILTIN_PARAMS.length-1}_load"
                                store = ir.createStore arg, allocas[i+1]
                                debug.log -> "store #{store}"
                                ir.createBr merge_bb

                                ir.setInsertPoint else_bb
                                ir.createBr merge_bb

                                ir.setInsertPoint merge_bb

                @iifeStack = []

                @finallyStack = []
                
                @visitWithScope new_scope, [n.body]

                # XXX more needed here - this lacks all sorts of control flow stuff.
                # Finish off the function.
                ir.createRet @loadUndefinedEjsValue()

                # insert an unconditional branch from entry_bb to body here, now that we're
                # sure we're not going to be inserting allocas into the entry_bb anymore.
                ir.setInsertPoint entry_bb
                ir.createBr body_bb
                        
                @currentFunction = null

                ir.setInsertPoint insertBlock

                return ir_func

        visitUnaryExpression: (n) ->
                debug.log -> "operator = '#{n.operator}'"

                builtin = "unop#{n.operator}"
                callee = @ejs_runtime[builtin]
        
                if n.operator is "delete"
                        if n.argument.type is syntax.MemberExpression
                                fake_literal = {
                                        type: syntax.Literal
                                        value: n.argument.property.name
                                        raw: "'#{n.argument.property.name}'"
                                }
                                return @createCall callee, [(@visitOrNull n.argument.object), (@visit fake_literal)], "result"
                        else
                                throw "unhandled delete syntax"
                else
                        if not callee
                                throw "Internal error: unary operator '#{n.operator}' not implemented"
                        @createCall callee, [@visitOrNull n.argument], "result"
                

        visitSequenceExpression: (n) ->
                rv = null
                for exp in n.expressions
                        rv = @visit exp
                rv
                
        visitBinaryExpression: (n) ->
                debug.log -> "operator = '#{n.operator}'"
                builtin = "binop#{n.operator}"
                callee = @ejs_runtime[builtin]
                if not callee
                        throw "Internal error: unhandled binary operator '#{n.operator}'"

                left_alloca = @createAlloca @currentFunction, types.EjsValue, "binop_left"
                left_visited = @visit n.left
                ir.createStore left_visited, left_alloca
                
                right_alloca = @createAlloca @currentFunction, types.EjsValue, "binop_right"
                right_visited = @visit n.right
                ir.createStore right_visited, right_alloca

                if n.left.is_constant? and n.right.is_constant?
                        console.warn "we could totally evaluate this at compile time, yo"
                        

                if left_visited.literal? and right_visited.literal?
                        if typeof left_visited.literal.value is "number" and typeof right_visited.literal.value is "number"
                                if n.operator is "<"
                                        return @loadBoolEjsValue left_visited.literal.value < right_visited.literal.value
                                        
                # call the actual runtime binaryop method
                call = @createCall callee, [(@createLoad left_alloca, "binop_left_load"), (@createLoad right_alloca, "binop_right_load")], "result_#{builtin}"
                call

        visitLogicalExpression: (n) ->
                debug.log -> "operator = '#{n.operator}'"
                result = @createAlloca @currentFunction, types.EjsValue, "result_#{n.operator}"

                left_visited = @visit n.left
                cond_truthy = @createCall @ejs_runtime.truthy, [left_visited], "cond_truthy"

                insertBlock = ir.getInsertBlock()
                insertFunc = insertBlock.parent
        
                left_bb  = new llvm.BasicBlock "cond_left", insertFunc
                right_bb  = new llvm.BasicBlock "cond_right", insertFunc
                merge_bb = new llvm.BasicBlock "cond_merge", insertFunc

                # we invert the test here - check if the condition is false/0
                cmp = ir.createICmpEq cond_truthy, falseConst(), "cmpresult"
                ir.createCondBr cmp, right_bb, left_bb

                ir.setInsertPoint left_bb
                # inside the else branch, left was truthy
                if n.operator is "||"
                        # for || we short circuit out here
                        ir.createStore left_visited, result
                else if n.operator is "&&"
                        # for && we evaluate the second and store it
                        ir.createStore (@visit n.right), result
                else
                        throw "Internal error 99.1"
                ir.createBr merge_bb

                ir.setInsertPoint right_bb
                # inside the then branch, left was falsy
                if n.operator is "||"
                        # for || we evaluate the second and store it
                        ir.createStore (@visit n.right), result
                else if n.operator is "&&"
                        # for && we short circuit out here
                        ir.createStore left_visited, result
                else
                        throw "Internal error 99.1"
                ir.createBr merge_bb

                ir.setInsertPoint merge_bb
                rv = @createLoad result, "result_#{n.operator}_load"

                ir.setInsertPoint merge_bb

                rv

        visitArgsForCall: (callee, pullThisFromArg0, args) ->
                argv = []

                args_offset = 0
                if callee.takes_builtins
                        args_offset = 1
                        if pullThisFromArg0 and args[0].type is syntax.MemberExpression
                                thisArg = @visit args[0].object
                                closure = @createPropertyLoad thisArg, args[0].property, args[0].computed
                        else
                                thisArg = @loadUndefinedEjsValue()
                                closure = @visit args[0]
                        
                        argv.push closure                                                   # %closure
                        argv.push thisArg                                                   # %this
                        argv.push int32Const args.length-1    # %argc. -1 because we pulled out the first arg to send as the closure

                if args.length > args_offset
                        argv.push @visitOrNull args[i] for i in [args_offset...args.length]

                argv

        visitArgsForConstruct: (callee, args) ->
                argv = []

                ctor = @visit args[0]

                proto = @createPropertyLoad ctor, { name: "prototype" }, false
                
                thisArg = @createCall @ejs_runtime.object_create, [proto], "objtmp"
                                                
                argv.push ctor                                                      # %closure
                argv.push thisArg                                                   # %this
                argv.push int32Const args.length-1    # %argc. -1 because we pulled out the first arg to send as the closure

                if args.length > 1
                        argv.push @visitOrNull args[i] for i in [1...args.length]

                argv
                                                                
        visitCallExpression: (n) ->
                debug.log -> "visitCall #{JSON.stringify n}"
                debug.log -> "          arguments length = #{n.arguments.length}"
                debug.log -> "          arguments[#{i}] =  #{JSON.stringify n.arguments[i]}" for i in [0...n.arguments.length]

                intrinsicHandler = @ejs_intrinsics[n.callee.name.slice 1]
                if not intrinsicHandler?
                        throw "Internal error: callee should not be null in visitCallExpression (callee = #{n.callee.name}, arguments = #{n.arguments.length})"

                intrinsicHandler.call @, n
                
        visitNewExpression: (n) ->
                if n.callee.type isnt syntax.Identifier or n.callee.name[0] isnt '%'
                        throw "invalid ctor #{JSON.stringify n.callee}"

                if n.callee.name isnt "%invokeClosure"
                        throw "new expressions may only have a callee of %invokeClosure, callee = #{n.callee.name}"
                        
                intrinsicHandler = @ejs_intrinsics[n.callee.name.slice 1]
                if not intrinsicHandler
                        throw "Internal error: ctor should not be null"

                intrinsicHandler.call @, n, true

        visitThisExpression: (n) ->
                debug.log "visitThisExpression"
                @createLoadThis()

        visitIdentifier: (n) ->
                debug.log -> "identifier #{n.name}"
                val = n.name
                source = @findIdentifierInScope val, @current_scope
                if source?
                        debug.log -> "found identifier in scope, at #{source}"
                        rv = @createLoad source, "load_#{val}"
                        return rv

                # special handling of the arguments object here, so we
                # only initialize/create it if the function is
                # actually going to use it.
                if val is "arguments"
                        arguments_alloca = @createAlloca @currentFunction, types.EjsValue, "local_arguments_object"
                        saved_insert_point = ir.getInsertBlock()
                        ir.setInsertPoint @currentFunction.entry_bb

                        load_argc = @createLoad @currentFunction.topScope["%argc"], "argc_load"
                        load_args = @createLoad @currentFunction.topScope["%args"], "args_load"

                        arguments_object = @createCall @ejs_runtime.arguments_new, [load_argc, load_args], "argstmp"
                        ir.createStore arguments_object, arguments_alloca
                        @currentFunction.topScope["arguments"] = arguments_alloca

                        ir.setInsertPoint saved_insert_point
                        rv = @createLoad arguments_alloca, "load_arguments"
                        return rv

                rv = null
                debug.log -> "calling getFunction for #{val}"
                rv = @module.getFunction val

                if not rv
                        debug.log -> "Symbol '#{val}' not found in current scope"
                        rv = @createPropertyLoad @loadGlobal(), n, false, false

                debug.log -> "returning #{rv}"
                rv

        visitObjectExpression: (n) ->
                obj = @createCall @ejs_runtime.object_create, [@loadNullEjsValue()], "objtmp"
                for property in n.properties
                        val = @visit property.value
                        key = property.key
                        @createPropertyStore obj, key, val, false
                obj

        visitArrayExpression: (n) ->
                obj = @createCall @ejs_runtime.array_new, [int32Const n.elements.length], "arrtmp"
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

        generateUCS2: (id, jsstr) ->
                ucsArrayType = llvm.ArrayType.get types.jschar, jsstr.length+1
                array_data = []
                (array_data.push jscharConst jsstr.charCodeAt i) for i in [0...jsstr.length]
                array_data.push jscharConst 0
                array = llvm.ConstantArray.get ucsArrayType, array_data
                arrayglobal = new llvm.GlobalVariable @module, ucsArrayType, "ucs2-#{id}", array
                arrayglobal

        generateEJSPrimString: (id, len) ->
                strglobal = new llvm.GlobalVariable @module, types.EjsPrimString, "primstring-#{id}", llvm.Constant.getAggregateZero types.EjsPrimString
                strglobal

        generateEJSValueForString: (id) ->
                name = "ejsval-string-#{id}"
                strglobal = new llvm.GlobalVariable @module, types.EjsValue, name, llvm.Constant.getAggregateZero types.EjsValue
                @module.getOrInsertGlobal name, types.EjsValue
                
        addStringLiteralInitialization: (name, ucs2, primstr, val, len) ->
                saved_insert_point = ir.getInsertBlock()

                ir.setInsertPointStartBB @literalInitializationBB
                strname = ir.createGlobalStringPtr name, "strname"

                arg0 = strname
                arg1 = val
                arg2 = primstr
                arg3 = ir.createInBoundsGetElementPointer ucs2, [(int32Const 0), (int32Const 0)], "ucs2"

                ir.createCall @ejs_runtime.init_string_literal, [arg0, arg1, arg2, arg3, int32Const len], ""

                ir.setInsertPoint saved_insert_point

        getAtom: (str) ->
                # check if it's an atom (a runtime library constant) first of all
                atom_name = "atom-#{str}"
                if @ejs_runtime[atom_name]?
                        return @createLoad @ejs_runtime[atom_name], "%str_atom_load"

                # if it's not, we create a constant and embed it in this module
        
                literal_key = "string-" + str
                if not @module_atoms[literal_key]?
                        literalId = genId()
                        ucs2_data = @generateUCS2 literalId, str
                        primstring = @generateEJSPrimString literalId, str.length
                        @module_atoms[literal_key] = @generateEJSValueForString literalId
                        @addStringLiteralInitialization str, ucs2_data, primstring, @module_atoms[literal_key], str.length

                strload = @createLoad @module_atoms[literal_key], "%literal_load"
                        
        visitLiteral: (n) ->
                # null literals, load _ejs_null
                if n.value is null
                        debug.log "literal: null"
                        return @loadNullEjsValue()

                        
                # undefined literals, load _ejs_undefined
                if n.value is undefined
                        debug.log "literal: undefined"
                        return @loadUndefinedEjsValue()

                # string literals
                if typeof n.raw is "string" and (n.raw[0] is '"' or n.raw[0] is "'")
                        debug.log -> "literal string: #{n.value}"

                        strload = @getAtom n.value
                        
                        strload.literal = n
                        debug.log -> "strload = #{strload}"
                        return strload

                # regular expression literals
                if typeof n.raw is "string" and n.raw[0] is '/'
                        debug.log -> "literal regexp: #{n.raw}"
                        c = ir.createGlobalStringPtr n.raw, "strconst"
                        regexpcall = @createCall @ejs_runtime.regexp_new_utf8, [c], "regexptmp"
                        debug.log -> "regexpcall = #{regexpcall}"
                        return regexpcall

                # number literals
                if typeof n.value is "number"
                        debug.log -> "literal number: #{n.value}"
                        if n.value is 0
                                numload = @createLoad @ejs_runtime['zero'], "load_zero"
                        else if n.value is 1
                                numload = @createLoad @ejs_runtime['one'], "load_one"
                        else
                                literal_key = "num-" + n.value
                                if @currentFunction.literalAllocas[literal_key]
                                        num_alloca = @currentFunction.literalAllocas[literal_key]
                                else
                                        # only create 1 instance of num literals used in a function, and allocate them in the entry block
                                        insertBlock = ir.getInsertBlock()

                                        ir.setInsertPoint @currentFunction.entry_bb
                                        num_alloca = ir.createAlloca types.EjsValue, "num-alloca-#{n.value}"
                                        c = llvm.ConstantFP.getDouble n.value
                                        call = @createCall @ejs_runtime.number_new, [c], "numconst-#{n.value}"
                                        ir.createStore call, num_alloca
                                        @currentFunction.literalAllocas[literal_key] = num_alloca
                                        
                                        ir.setInsertPoint insertBlock
                                
                                numload = @createLoad num_alloca, "%num_alloca"
                        numload.literal = n
                        debug.log -> "numload = #{numload}"
                        return numload

                # boolean literals
                if typeof n.value is "boolean"
                        debug.log -> "literal boolean: #{n.value}"
                        return @loadBoolEjsValue n.value

                throw "Internal error: unrecognized literal of type #{typeof n.value}"

        createCall: (callee, argv, callname, canThrow) ->
                # if we're inside a try block we have to use createInvoke, and pass two basic blocks:
                #   the normal block, which is basically this IR instruction's continuation
                #   the unwind block, where we land if the call throws an exception.
                #
                # for some builtins we know they won't throw, so we can skip this step and still use createCall.
                if TryExitableScope.unwindStack.length is 0 or callee.doesNotThrow or not canThrow
                        calltmp = ir.createCall callee, argv, callname
                        calltmp.setDoesNotThrow() if callee.doesNotThrow
                        calltmp.setDoesNotAccessMemory() if callee.doesNotAccessMemory
                        calltmp.setOnlyReadsMemory() if not callee.doesNotAccessMemory and callee.onlyReadsMemory
                else
                        insertBlock = ir.getInsertBlock()
                        insertFunc = insertBlock.parent
                        normal_block  = new llvm.BasicBlock "normal", insertFunc
                        calltmp = ir.createInvoke callee, argv, normal_block, TryExitableScope.unwindStack[0].getLandingBlock(), callname
                        calltmp.setDoesNotThrow() if callee.doesNotThrow
                        calltmp.setDoesNotAccessMemory() if callee.doesNotAccessMemory
                        calltmp.setOnlyReadsMemory() if not callee.doesNotAccessMemory and callee.onlyReadsMemory
                        # after we've made our call we need to change the insertion point to our continuation
                        ir.setInsertPoint normal_block
                calltmp
        
        visitThrow: (n) ->
                arg = @visit n.argument
                @createCall @ejs_runtime.throw, [arg], "", true

        visitTry: (n) ->
                insertBlock = ir.getInsertBlock()
                insertFunc = insertBlock.parent

                # the alloca that stores the reason we ended up in the finally block
                @currentFunction.cleanup_reason = @createAlloca @currentFunction, types.int32, "cleanup_reason" unless @currentFunction.cleanup_reason?

                # the merge bb where everything branches to after falling off the end of a catch/finally block
                merge_block = new llvm.BasicBlock "try_merge", insertFunc

                # if we have a finally clause, create finally_block
                if n.finalizer?
                        finally_block = new llvm.BasicBlock "finally_bb", insertFunc
                        branch_target = finally_block
                        @finallyStack.unshift finally_block
                else
                        branch_target = merge_block

                scope = new TryExitableScope @currentFunction.cleanup_reason, branch_target, (-> new llvm.BasicBlock "exception", insertFunc)
                scope.enter()

                console.log "visiting try {}"
                @visit n.block
                console.log "done visiting try {}"

                if n.finalizer?
                        @finallyStack.shift()
                
                # at the end of the try block branch to our branch_target (either the finally block or the merge block after the try{}) with REASON_FALLOFF
                scope.exitAft false

                if scope.landing_pad_block?
                        # the scope's landingpad block is created if needed by @createCall (using that function we pass in as the last argument to TryExitableScope's ctor.)
                        # if a try block includes no calls, there's no need for an landing pad block as nothing can throw, and we don't bother generating any code for the
                        # catch clause.
                        console.log "have an unwind block"
                        ir.setInsertPoint scope.unwind_block

                        # XXX is it an error to have multiple catch handlers, as JS doesn't allow you to filter by type?
                        clause_count = if n.handlers.length > 0 then 1 else 0
                        
                        casted_personality = ir.createPointerCast @ejs_runtime.personality, types.int8Pointer, "personality"
                        caught_result = ir.createLandingPad types.EjsValue.pointerTo(), casted_personality, clause_count, "caught_result"
                        caught_result.addClause types.EjsValue.pointerTo() # this used to be '@ejs_runtime.exception_typeinfo'
                        caught_result.setCleanup true
                
                        # if we have a catch clause, create catch_bb
                        if n.handlers.length > 0
                                catch_block = new llvm.BasicBlock "catch_bb", insertFunc
                                ir.setInsertPoint catch_block

                                # call _ejs_begin_catch
                                catchval = ir.createCall @ejs_runtime.begin_catch, [(ir.createPointerCast caught_result, types.int8Pointer, "")], "catchval"

                                # create a new scope which maps the catch parameter name (the "e" in try { } catch (e) { }) to catchval
                                catch_scope = Object.create null
                                if n.handlers[0].param?.name?
                                        catch_name = n.handlers[0].param.name
                                        alloca = @createAlloca @currentFunction, types.EjsValue, "local_catch_#{catch_name}"
                                        catch_scope[catch_name] = alloca
                                        ir.createStore catchval, alloca

                                # and visit the handler with this scope active
                                @visitWithScope catch_scope, [n.handlers[0]]

                                # unsure about this one - we should likely call end_catch if another exception is thrown from the catch block?
                                ir.createCall @ejs_runtime.end_catch, [], ""

                                # if we make it to the end of the catch block, branch unconditionally to the branch target (either this try's
                                # finally block or the merge pointer after the try)
                                ir.createBr branch_target

                        # Unwind Resume Block (calls _Unwind_Resume)
                        unwind_resume_block = new llvm.BasicBlock "unwind_resume", insertFunc
                        ir.setInsertPoint unwind_resume_block
                        ir.createResume caught_result

                        if catch_block?
                                ir.createBr catch_block
                        else if finally_block?
                                ir.createBr finally_block
                        else
                                throw "this shouldn't happen.  a try{} without either a catch{} or finally{}"

                scope.leave()

                # Finally Block
                if n.finalizer?
                        ir.setInsertPoint finally_block
                        @visit n.finalizer

                        cleanup_reason = @createLoad @currentFunction.cleanup_reason, "cleanup_reason_load"

                        if @returnValueAlloca?
                                return_tramp = new llvm.BasicBlock "return_tramp", insertFunc
                                ir.setInsertPoint return_tramp
                                
                                if @finallyStack.length > 0
                                        ir.createStore (int32Const ExitableScope.REASON_RETURN), @currentFunction.cleanup_reason
                                        ir.createBr @finallyStack[0]
                                else
                                        rv = @createLoad @returnValueAlloca, "rv"
                                        ir.createRet rv
                        
                        ir.setInsertPoint finally_block
                        switch_stmt = ir.createSwitch cleanup_reason, merge_block, scope.destinations.length + 1
                        if @returnValueAlloca?
                                switch_stmt.addCase (int32Const ExitableScope.REASON_RETURN), return_tramp
                        
                console.log "done with try block"
                ir.setInsertPoint merge_block

        handleInvokeClosureIntrinsic: (exp, ctor_context= false) ->
                if ctor_context
                        argv = @visitArgsForConstruct @ejs_runtime.invoke_closure, exp.arguments
                else
                        argv = @visitArgsForCall @ejs_runtime.invoke_closure, true, exp.arguments

                modified_argv = (argv[n] for n in [0...BUILTIN_PARAMS.length])

                if argv.length > BUILTIN_PARAMS.length
                        argv = argv.slice BUILTIN_PARAMS.length
                        argv.forEach (a,i) =>
                                gep = ir.createGetElementPointer @currentFunction.scratch_area, [(int32Const 0), (int64Const i)], "arg_gep_#{i}"
                                store = ir.createStore argv[i], gep, "argv[#{i}]-store"

                        argsCast = ir.createGetElementPointer @currentFunction.scratch_area, [(int32Const 0), (int64Const 0)], "call_args_load"
                                                
                        modified_argv[BUILTIN_PARAMS.length] = argsCast

                else
                        modified_argv[BUILTIN_PARAMS.length] = nullConst types.EjsValue.pointerTo()
                                
                argv = modified_argv
                call_result = @createAlloca @currentFunction, types.EjsValue, "call_result"

                if decompose_closure_on_invoke
                        insertBlock = ir.getInsertBlock()
                        insertFunc = insertBlock.parent

                        # inline decomposition of the closure (with a fallback to the runtime invoke if there are bound args)
                        # and direct call
                        runtime_invoke_bb = new llvm.BasicBlock "runtime_invoke_bb", insertFunc
                        direct_invoke_bb = new llvm.BasicBlock "direct_invoke_bb", insertFunc
                        invoke_merge_bb = new llvm.BasicBlock "invoke_merge_bb", insertFunc

                        func_alloca = @createAlloca @currentFunction, types.EjsClosureFunc, "direct_invoke_func"
                        env_alloca = @createAlloca @currentFunction, types.EjsClosureEnv, "direct_invoke_env"
                        this_alloca = @createAlloca @currentFunction, types.EjsValue, "direct_invoke_this"

                        # provide the default "this" for decompose_closure.  if one was bound it'll get overwritten
                        ir.createStore argv[1], this_alloca
                        
                        decompose_args = [ argv[0], func_alloca, env_alloca, this_alloca ]
                        decompose_rv = @createCall @ejs_runtime.decompose_closure, decompose_args, "decompose_rv", false
                        cmp = ir.createICmpEq decompose_rv, falseConst(), "cmpresult"
                        ir.createCondBr cmp, runtime_invoke_bb, direct_invoke_bb

                        # if there were bound args we have to fall back to the runtime invoke method (since we can't
                        # guarantee enough room in our scratch area -- should we inline a check here or pass the length
                        # of the scratch area to decompose?  perhaps...FIXME)
                        # 
                        ir.setInsertPoint runtime_invoke_bb
                        calltmp = @createCall @ejs_runtime.invoke_closure, argv, "calltmp"
                        store = ir.createStore calltmp, call_result
                        ir.createBr invoke_merge_bb

                        # in the successful case we modify our argv with the responses and directly invoke the closure func
                        ir.setInsertPoint direct_invoke_bb
                        direct_args = [ (@createLoad env_alloca, "env"), (@createLoad this_alloca, "this"), argv[2], argv[3] ]
                        calltmp = @createCall (@createLoad func_alloca, "func"), direct_args, "calltmp"
                        store = ir.createStore calltmp, call_result
                        ir.createBr invoke_merge_bb

                        ir.setInsertPoint invoke_merge_bb
                else
                        calltmp = @createCall @ejs_runtime.invoke_closure, argv, "calltmp"
                        store = ir.createStore calltmp, call_result
        
                if ctor_context
                        argv[1]
                else
                        @createLoad call_result, "call_result_load"
                        
                        
                        
                
        handleMakeClosureIntrinsic: (exp, ctor_context= false) ->
                argv = @visitArgsForCall @ejs_runtime.make_closure, false, exp.arguments
                closure_result = @createAlloca @currentFunction, types.EjsValue, "closure_result"
                calltmp = @createCall @ejs_runtime.make_closure, argv, "closure_tmp"
                store = ir.createStore calltmp, closure_result
                @createLoad closure_result, "closure_result_load"

        handleMakeAnonClosureIntrinsic: (exp, ctor_context= false) ->
                argv = @visitArgsForCall @ejs_runtime.make_anon_closure, false, exp.arguments
                closure_result = @createAlloca @currentFunction, types.EjsValue, "closure_result"
                calltmp = @createCall @ejs_runtime.make_anon_closure, argv, "closure_tmp"
                store = ir.createStore calltmp, closure_result
                @createLoad closure_result, "closure_result_load"
                                
                
        handleCreateArgScratchAreaIntrinsic: (exp, ctor_context= false) ->
                argsArrayType = llvm.ArrayType.get types.EjsValue, exp.arguments[0].value
                @currentFunction.scratch_area = @createAlloca @currentFunction, argsArrayType, "args_scratch_area"
                

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

                # set the types of all later arguments to be types.EjsValue
                param.llvm_type = types.EjsValue for param in n.params[BUILTIN_PARAMS.length..]

                # the LLVMIR func we allocate takes the proper EJSValue** parameter in the 4th spot instead of all the parameters
                n.ir_func = takes_builtins @module.getOrInsertFunction n.ir_name, types.EjsValue, (param.llvm_type for param in BUILTIN_PARAMS).concat [types.EjsValue.pointerTo()]

                # enable shadow stack map for gc roots
                #n.ir_func.setGC "shadow-stack"
                
                ir_args = n.ir_func.args
                (ir_args[i].setName n.params[i].name) for i in [0...BUILTIN_PARAMS.length]
                ir_args[BUILTIN_PARAMS.length].setName "%args"

                # we don't need to recurse here since we won't have nested functions at this point
                n

sanitize_with_regexp = (filename) ->
        filename.replace /[.,-\/\\]/g, "_" # this is insanely inadequate

sanitize_with_replace = (filename) ->
        replace_all = (str, from, to) ->
                while (str.indexOf from) > -1
                        str = str.replace from, to
                str

        filename = replace_all filename, ".", "_"
        filename = replace_all filename, ",", "_"
        filename = replace_all filename, "-", "_"
        filename = replace_all filename, "/", "_"
        filename = replace_all filename, "\\", "_"
        
insert_toplevel_func = (tree, filename) ->
        sanitize = if __ejs? then sanitize_with_replace else sanitize_with_regexp
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

exports.compile = (tree, base_output_filename, source_filename) ->

        console.warn "compiling #{source_filename} -> #{base_output_filename}"
        
        tree = insert_toplevel_func tree, base_output_filename

        debug.log -> escodegen.generate tree

        toplevel_name = tree.body[0].id.name
        
        debug.log 1, "before closure conversion"
        debug.log 1, -> escodegen.generate tree
        
        tree = closure_conversion.convert tree, path.basename source_filename

        debug.log 1, "after closure conversion"
        debug.log 1, -> escodegen.generate tree
        
        module = new llvm.Module "compiled-#{base_output_filename}"

        module.toplevel_name = toplevel_name

        visitor = new AddFunctionsVisitor module
        tree = visitor.visit tree

        debug.log -> escodegen.generate tree

        visitor = new LLVMIRVisitor module, source_filename
        visitor.visit tree

        module
