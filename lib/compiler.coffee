esprima = require 'esprima'
escodegen = require 'escodegen'
syntax = esprima.Syntax
debug = require 'debug'
path = require 'path'

#debug.setLevel if __ejs? then 1 else 0

{ Set } = require 'set'
{ NodeVisitor } = require 'nodevisitor'
closure_conversion = require 'closure-conversion'
{ desugar } = require 'echo-desugar'

llvm = require 'llvm'
irbuilder = llvm.IRBuilder

# set to true to inline more of the call sequence at call sites (we still have to call into the runtime to decompose the closure itself for now)
# disable this for now because it breaks more of the exception tests
decompose_closure_on_invoke = false

# special key for parent scope when performing lookups
PARENT_SCOPE_KEY = ":parent:"

stringType = llvm.Type.getInt8Ty().pointerTo()
int8PointerType = stringType
boolType   = llvm.Type.getInt8Ty()
voidType   = llvm.Type.getVoidTy()
int32Type  = llvm.Type.getInt32Ty()
int64Type  = llvm.Type.getInt64Ty()

int32Constant = (c) ->
        constant = llvm.Constant.getIntegerValue int32Type, c
        constant.is_constant = true
        constant.constant_val = c
        constant
        
int64Constant = (c) ->
        constant = llvm.Constant.getIntegerValue int64Type, c
        constant.is_constant = true
        constant.constant_val = c
        constant
        
boolConstant  = (c) ->
        constant = llvm.Constant.getIntegerValue boolType, if c is false then 0 else 1
        constant.is_constant = true
        constant.constant_val = c
        constant
        

EjsValueType = int64Type
EjsClosureEnvType = EjsValueType
EjsPropIteratorType = EjsValueType
EjsClosureFuncType = (llvm.FunctionType.get EjsValueType, [EjsClosureEnvType, EjsValueType, llvm.Type.getInt32Ty(), EjsValueType.pointerTo()]).pointerTo()

# exception types

# the c++ typeinfo for our exceptions
EjsExceptionTypeInfoType = (llvm.StructType.create "EjsExceptionTypeInfoType", [int8PointerType, int8PointerType, int8PointerType]).pointerTo()

BUILTIN_PARAMS = [
  { type: syntax.Identifier, name: "%closure", llvm_type: EjsClosureEnvType }
  { type: syntax.Identifier, name: "%this",    llvm_type: EjsValueType }
  { type: syntax.Identifier, name: "%argc",    llvm_type: int32Type }
]

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

class ExitableScope
        @REASON_RETURN: -10
        
        @scopeStack: null
        
        constructor: (@label = null) ->
                @parent = null

        exitFore: (label = null) ->
                throw "Exitable scope doesn't allow exitFore"

        exitAft: (fromBreak, label = null) ->
                throw "Exitable scope doesn't allow exitAft"

        enter: ->
                @parent = ExitableScope.scopeStack
                ExitableScope.scopeStack = @

        leave: ->
                ExitableScope.scopeStack = @parent
                @parent = null
                
class TryExitableScope extends ExitableScope
        REASON_FALLOFF_TRY:    -2  # we fell off the end of the try block
        REASON_ERROR:          -1  # error condition

        REASON_BREAK: "break"
        REASON_CONTINUE: "continue"
        
        @unwindStack: []
                
        constructor: (@cleanup_alloca, @cleanup_bb, @create_unwind_bb) ->
                @isTry = true
                @destinations = []
                super()

        enter: ->
                TryExitableScope.unwindStack.unshift @
                super
                
        leave: ->
                TryExitableScope.unwindStack.shift()
                super

        getUnwindBlock: ->
                return @unwind_block if @unwind_block?
                @unwind_block = @create_unwind_bb()
                
        
        lookupDestinationIdForScope: (scope, reason) ->
                for i in [0...@destinations.length]
                        if @destinations[i].scope is scope and @destinations[i].reason is reason
                                return @destinations[i].id

                id = int32Constant @destinations.length
                @destinations.unshift scope: scope, reason: reason, id: id
                id
                
        
        exitFore: (label = null) ->
                if label?
                        scope = LoopExitableScope.findLabeled label
                else
                        scope = LoopExitableScope.findFirst()
                        
                reason = @lookupDestinationIdForScope scope, @REASON_CONTINUE 
                irbuilder.createStore reason, @cleanup_alloca
                irbuilder.createBr @cleanup_bb

        exitAft: (fromBreak, label = null) ->
                if fromBreak
                        if label?
                                scope = LoopExitableScope.findLabeled label
                        else
                                scope = TryExitableScope.findFirstNonTry()
                        
                        reason = @lookupDestinationIdForScope scope, @REASON_BREAK
                else
                        reason = int32Constant @REASON_FALLOFF_TRY

                irbuilder.createStore reason, @cleanup_alloca
                irbuilder.createBr @cleanup_bb

        @findFirstNonTry: (stack = ExitableScope.scopeStack) ->
                return stack if not stack.isTry
                return @findFirstNonTry stack.parent

class SwitchExitableScope extends ExitableScope
        constructor: (@merge_bb) ->
                super()

        exitAft: (fromBreak, label = null) ->
                irbuilder.createBr @merge_bb

class LoopExitableScope extends ExitableScope
        constructor: (label, @fore_bb, @aft_bb) ->
                @isLoop = true
                super label

        exitFore: (label = null) ->
                irbuilder.createBr @fore_bb
                
        exitAft: (fromBreak, label = null) ->
                irbuilder.createBr @aft_bb

        @findLabeled: (l, stack = ExitableScope.scopeStack) ->
                return stack if l is stack.label
                return LoopExitableScope.findLabeled l, stack.parent

        @findFirst: (stack = ExitableScope.scopeStack) ->
                return stack if stack.isLoop
                return LoopExitableScope.findFirst stack.parent

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
                        personality:           module.getOrInsertExternalFunction "__ejs_personality_v0",           int32Type, [int32Type, int32Type, int64Type, int8PointerType, int8PointerType]

                        invoke_closure:        takes_builtins module.getOrInsertExternalFunction "_ejs_invoke_closure", EjsValueType, [EjsValueType, EjsValueType, int32Type, EjsValueType.pointerTo()]
                        make_closure:          module.getOrInsertExternalFunction "_ejs_function_new", EjsValueType, [EjsClosureEnvType, EjsValueType, EjsClosureFuncType]
                        make_anon_closure:     module.getOrInsertExternalFunction "_ejs_function_new_anon", EjsValueType, [EjsClosureEnvType, EjsClosureFuncType]
                        decompose_closure:     module.getOrInsertExternalFunction "_ejs_decompose_closure", boolType, [EjsValueType, EjsClosureFuncType.pointerTo(), EjsClosureEnvType.pointerTo(), EjsValueType.pointerTo()]
                                                
                        object_create:         module.getOrInsertExternalFunction "_ejs_object_create",             EjsValueType, [EjsValueType]
                        arguments_new:         module.getOrInsertExternalFunction "_ejs_arguments_new",             EjsValueType, [int32Type, EjsValueType.pointerTo()]
                        array_new:             module.getOrInsertExternalFunction "_ejs_array_new",                 EjsValueType, [int32Type]
                        number_new:            does_not_throw does_not_access_memory module.getOrInsertExternalFunction "_ejs_number_new",                EjsValueType, [llvm.Type.getDoubleTy()]
                        string_new_utf8:       only_reads_memory does_not_throw module.getOrInsertExternalFunction "_ejs_string_new_utf8",           EjsValueType, [stringType]
                        regexp_new_utf8:       module.getOrInsertExternalFunction "_ejs_regexp_new_utf8",           EjsValueType, [stringType]
                        truthy:                does_not_throw does_not_access_memory module.getOrInsertExternalFunction "_ejs_truthy",                    boolType, [EjsValueType]
                        object_setprop:        module.getOrInsertExternalFunction "_ejs_object_setprop",            EjsValueType, [EjsValueType, EjsValueType, EjsValueType]
                        object_getprop:        only_reads_memory module.getOrInsertExternalFunction "_ejs_object_getprop",           EjsValueType, [EjsValueType, EjsValueType]
                        object_getprop_utf8:   only_reads_memory module.getOrInsertExternalFunction "_ejs_object_getprop_utf8",      EjsValueType, [EjsValueType, stringType]
                        object_setprop_utf8:   module.getOrInsertExternalFunction "_ejs_object_setprop_utf8",       EjsValueType, [EjsValueType, stringType, EjsValueType]
                        prop_iterator_new:     module.getOrInsertExternalFunction "_ejs_property_iterator_new",     EjsPropIteratorType, [EjsValueType]
                        prop_iterator_current: module.getOrInsertExternalFunction "_ejs_property_iterator_current", EjsValueType, [EjsPropIteratorType]
                        prop_iterator_next:    module.getOrInsertExternalFunction "_ejs_property_iterator_next",    boolType, [EjsPropIteratorType, boolType]
                        prop_iterator_free:    module.getOrInsertExternalFunction "_ejs_property_iterator_free",    voidType, [EjsPropIteratorType]
                        throw:                 module.getOrInsertExternalFunction "_ejs_throw",                     voidType, [EjsValueType]
                        rethrow:               module.getOrInsertExternalFunction "_ejs_rethrow",                   voidType, [EjsValueType]
                        
                        undefined:             module.getOrInsertGlobal           "_ejs_undefined",                 EjsValueType
                        "true":                module.getOrInsertGlobal           "_ejs_true",                      EjsValueType
                        "false":               module.getOrInsertGlobal           "_ejs_false",                     EjsValueType
                        "null":                module.getOrInsertGlobal           "_ejs_null",                      EjsValueType
                        "one":                 module.getOrInsertGlobal           "_ejs_one",                       EjsValueType
                        "zero":                module.getOrInsertGlobal           "_ejs_zero",                      EjsValueType
                        "atom-length":         module.getOrInsertGlobal           "_ejs_atom_length",               EjsValueType
                        "atom-object":         module.getOrInsertGlobal           "_ejs_atom_object",               EjsValueType
                        "atom-function":       module.getOrInsertGlobal           "_ejs_atom_function",             EjsValueType
                        global:                module.getOrInsertGlobal           "_ejs_global",                    EjsValueType
                        exception_typeinfo:    module.getOrInsertGlobal           "EJS_EHTYPE_ejsvalue",            EjsExceptionTypeInfoType
                        
                        "unop-":           module.getOrInsertExternalFunction "_ejs_op_neg",         EjsValueType, [EjsValueType]
                        "unop+":           module.getOrInsertExternalFunction "_ejs_op_plus",        EjsValueType, [EjsValueType]
                        "unop!":           module.getOrInsertExternalFunction "_ejs_op_not",         EjsValueType, [EjsValueType]
                        "unop~":           module.getOrInsertExternalFunction "_ejs_op_bitwise_not", EjsValueType, [EjsValueType]
                        "unoptypeof":      does_not_throw module.getOrInsertExternalFunction "_ejs_op_typeof",      EjsValueType, [EjsValueType]
                        "unopdelete":      module.getOrInsertExternalFunction "_ejs_op_delete",      EjsValueType, [EjsValueType, EjsValueType] # this is a unop, but ours only works for memberexpressions
                        "unopvoid":        module.getOrInsertExternalFunction "_ejs_op_void",        EjsValueType, [EjsValueType]
                        "binop^":          module.getOrInsertExternalFunction "_ejs_op_bitwise_xor", EjsValueType, [EjsValueType, EjsValueType]
                        "binop&":          module.getOrInsertExternalFunction "_ejs_op_bitwise_and", EjsValueType, [EjsValueType, EjsValueType]
                        "binop|":          module.getOrInsertExternalFunction "_ejs_op_bitwise_or",  EjsValueType, [EjsValueType, EjsValueType]
                        "binop>>":         module.getOrInsertExternalFunction "_ejs_op_rsh",         EjsValueType, [EjsValueType, EjsValueType]
                        "binop<<":         module.getOrInsertExternalFunction "_ejs_op_lsh",         EjsValueType, [EjsValueType, EjsValueType]
                        "binop>>>":        module.getOrInsertExternalFunction "_ejs_op_ursh",        EjsValueType, [EjsValueType, EjsValueType]
                        "binop<<<":        module.getOrInsertExternalFunction "_ejs_op_ulsh",        EjsValueType, [EjsValueType, EjsValueType]
                        "binop%":          module.getOrInsertExternalFunction "_ejs_op_mod",         EjsValueType, [EjsValueType, EjsValueType]
                        "binop+":          module.getOrInsertExternalFunction "_ejs_op_add",         EjsValueType, [EjsValueType, EjsValueType]
                        "binop*":          module.getOrInsertExternalFunction "_ejs_op_mult",        EjsValueType, [EjsValueType, EjsValueType]
                        "binop/":          module.getOrInsertExternalFunction "_ejs_op_div",         EjsValueType, [EjsValueType, EjsValueType]
                        "binop<":          module.getOrInsertExternalFunction "_ejs_op_lt",          EjsValueType, [EjsValueType, EjsValueType]
                        "binop<=":         module.getOrInsertExternalFunction "_ejs_op_le",          EjsValueType, [EjsValueType, EjsValueType]
                        "binop>":          module.getOrInsertExternalFunction "_ejs_op_gt",          EjsValueType, [EjsValueType, EjsValueType]
                        "binop>=":         module.getOrInsertExternalFunction "_ejs_op_ge",          EjsValueType, [EjsValueType, EjsValueType]
                        "binop-":          module.getOrInsertExternalFunction "_ejs_op_sub",         EjsValueType, [EjsValueType, EjsValueType]
                        "binop===":        does_not_throw does_not_access_memory module.getOrInsertExternalFunction "_ejs_op_strict_eq",   EjsValueType, [EjsValueType, EjsValueType]
                        "binop==":         module.getOrInsertExternalFunction "_ejs_op_eq",          EjsValueType, [EjsValueType, EjsValueType]
                        "binop!==":        does_not_throw does_not_access_memory module.getOrInsertExternalFunction "_ejs_op_strict_neq",  EjsValueType, [EjsValueType, EjsValueType]
                        "binop!=":         module.getOrInsertExternalFunction "_ejs_op_neq",         EjsValueType, [EjsValueType, EjsValueType]
                        "binopinstanceof": module.getOrInsertExternalFunction "_ejs_op_instanceof",  EjsValueType, [EjsValueType, EjsValueType]
                        "binopin":         module.getOrInsertExternalFunction "_ejs_op_in",          EjsValueType, [EjsValueType, EjsValueType]
                }

                @initGlobalScope();

        # lots of helper methods

        loadBoolEjsValue: (n) ->
                boolval = irbuilder.createLoad (if n then @ejs_runtime['true'] else @ejs_runtime['false']), "load_bool"
                boolval.is_constant = true
                boolval.constant_val = n
                boolval
                
        loadNullEjsValue: ->
                nullval = irbuilder.createLoad @ejs_runtime['null'], "load_null"
                nullval.is_constant = true
                nullval.constant_val = null
                nullval
        loadUndefinedEjsValue: ->
                undef = irbuilder.createLoad @ejs_runtime.undefined, "load_undefined"
                undef.is_constant = true
                undef.constant_val = undefined
                undef
        loadGlobal: -> irbuilder.createLoad @ejs_runtime.global, "load_global"

        pushIIFEInfo: (info) ->
                @iifeStack.unshift info
                
        popIIFEInfo: ->
                @iifeStack.shift()

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
                null

                                
        createAlloca: (func, type, name) ->
                saved_insert_point = irbuilder.getInsertBlock()
                irbuilder.setInsertPointStartBB func.entry_bb
                alloca = irbuilder.createAlloca type, name
                #if type is EjsValueType
                #        # EjsValues are rooted
                #        @createCall @llvm_intrinsics.gcroot, [(irbuilder.createPointerCast alloca, int8PointerType.pointerTo(), "rooted_alloca"), llvm.Constant.getNull int8PointerType], ""

                irbuilder.setInsertPoint saved_insert_point
                alloca

        createAllocas: (func, ids, scope) ->
                allocas = []
                new_allocas = []
                
                # the allocas are always allocated in the function entry_bb so the mem2reg opt pass can regenerate the ssa form for us
                saved_insert_point = irbuilder.getInsertBlock()
                irbuilder.setInsertPointStartBB func.entry_bb

                j = 0
                for i in [0...ids.length]
                        name = ids[i].id.name
                        if !scope.hasOwnProperty name
                                allocas[j] = irbuilder.createAlloca EjsValueType, "local_#{name}"
                                scope[name] = allocas[j]
                                new_allocas[j] = true
                        else
                                allocas[j] = scope[name]
                                new_allocas[j] = false
                        j = j + 1
                                

                # reinstate the IRBuilder to its previous insert point so we can insert the actual initializations
                irbuilder.setInsertPoint saved_insert_point

                { allocas: allocas, new_allocas: new_allocas }

        createPropertyStore: (obj,prop,rhs,computed) ->
                if computed
                        # we store obj[prop], prop can be any value
                        prop_alloca = @createAlloca @currentFunction, EjsValueType, "prop_alloca"
                        irbuilder.createStore (@visit prop), prop_alloca
                        @createCall @ejs_runtime.object_setprop, [obj, (irbuilder.createLoad prop_alloca, "%prop_alloca"), rhs], "propstore_computed"
                else
                        # we store obj.prop, prop is an id
                        if prop.type is syntax.Identifier
                                pname = prop.name
                        else # prop.type is syntax.Literal
                                pname = prop.value

                        debug.log "createPropertyStore #{obj}[#{pname}]"
                        
                        c = irbuilder.createGlobalStringPtr pname, "strconst"
                        @createCall @ejs_runtime.object_setprop_utf8, [obj, c, rhs], "propstore_#{pname}"
                
        createPropertyLoad: (obj,prop,computed,canThrow = true) ->
                if computed
                        # we load obj[prop], prop can be any value
                        loadprop = @visit prop
                        pname = "computed"
                        @createCall @ejs_runtime.object_getprop, [obj, loadprop], "getprop_#{pname}"
                else
                        # we load obj.prop, prop is an id
                        pname = prop.name
                        c = irbuilder.createGlobalStringPtr pname, "propname_#{pname}"
                        @createCall @ejs_runtime.object_getprop_utf8, [obj, c], "getprop_#{pname}", canThrow
                

        createLoadThis: () ->
                _this = @findIdentifierInScope "%this", @current_scope
                return irbuilder.createLoad _this, "load_this"


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
                        insertBlock = irbuilder.getInsertBlock()
                        insertFunc = insertBlock.parent
                        
                        iife_rv = @createAlloca @currentFunction, EjsValueType, "%iife_rv"
                        iife_bb = new llvm.BasicBlock "iife_dest", insertFunc

                @pushIIFEInfo iife_rv: iife_rv, iife_dest_bb: iife_bb

                @visitWithScope new_scope, n.body

                @popIIFEInfo()
                if iife_bb
                        irbuilder.createBr iife_bb
                        irbuilder.setInsertPoint iife_bb
                        rv = irbuilder.createLoad iife_rv, "%iife_rv_load"
                        rv
                else
                        n

        visitSwitch: (n) ->
                insertBlock = irbuilder.getInsertBlock()
                insertFunc = insertBlock.parent

                switch_bb = new llvm.BasicBlock "switch", insertFunc

                irbuilder.createBr switch_bb
                irbuilder.setInsertPoint switch_bb
                
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
                irbuilder.createBr case_checks[0].dest_check
                irbuilder.setInsertPoint case_checks[0].dest_check
                for casenum in [0...case_checks.length-1]
                        test = @visit case_checks[casenum].test
                        discTest = @createCall @ejs_runtime["binop==="], [discr, test], "test"
                        disc_truthy = @createCall @ejs_runtime.truthy, [discTest], "disc_truthy"
                        disc_cmp = irbuilder.createICmpEq disc_truthy, (boolConstant false), "disccmpresult"
                        irbuilder.createCondBr disc_cmp, case_checks[casenum+1].dest_check, case_checks[casenum].body
                        irbuilder.setInsertPoint case_checks[casenum+1].dest_check


                case_bodies = []
                
                # now insert all the code for the case consequents
                for _case in n.cases
                        case_bodies.push bb:_case.bb, consequent:_case.consequent

                case_bodies.push bb:merge_bb
                
                for casenum in [0...case_bodies.length-1]
                        irbuilder.setInsertPoint case_bodies[casenum].bb
                        for c of case_bodies[casenum].consequent
                                @visit case_bodies[casenum].consequent[c]
                        irbuilder.createBr case_bodies[casenum+1].bb
                        
                irbuilder.setInsertPoint merge_bb

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
                insertBlock = irbuilder.getInsertBlock()
                insertFunc = insertBlock.parent

                init_bb = new llvm.BasicBlock "for_init", insertFunc
                test_bb = new llvm.BasicBlock "for_test", insertFunc
                body_bb = new llvm.BasicBlock "for_body", insertFunc
                update_bb = new llvm.BasicBlock "for_update", insertFunc
                merge_bb = new llvm.BasicBlock "for_merge", insertFunc

                irbuilder.createBr init_bb
                
                irbuilder.setInsertPoint init_bb
                @visit n.init
                irbuilder.createBr test_bb

                irbuilder.setInsertPoint test_bb
                if n.test
                        cond_truthy = @createCall @ejs_runtime.truthy, [@visit(n.test)], "cond_truthy"
                        cmp = irbuilder.createICmpEq cond_truthy, (boolConstant false), "cmpresult"
                        irbuilder.createCondBr cmp, merge_bb, body_bb
                else
                        irbuilder.createBr body_bb

                scope = new LoopExitableScope n.label, update_bb, merge_bb
                scope.enter()

                irbuilder.setInsertPoint body_bb
                @visit n.body
                irbuilder.createBr update_bb

                irbuilder.setInsertPoint update_bb
                @visit n.update
                irbuilder.createBr test_bb

                scope.leave()

                irbuilder.setInsertPoint merge_bb
                merge_bb
                                
        visitWhile: (n) ->
                insertBlock = irbuilder.getInsertBlock()
                insertFunc = insertBlock.parent
                
                while_bb  = new llvm.BasicBlock "while_start", insertFunc
                body_bb = new llvm.BasicBlock "while_body", insertFunc
                merge_bb = new llvm.BasicBlock "while_merge", insertFunc

                irbuilder.createBr while_bb
                irbuilder.setInsertPoint while_bb
                
                cond_truthy = @createCall @ejs_runtime.truthy, [@visit(n.test)], "cond_truthy"
                cmp = irbuilder.createICmpEq cond_truthy, (boolConstant false), "cmpresult"
                
                irbuilder.createCondBr cmp, merge_bb, body_bb

                scope = new LoopExitableScope n.label, while_bb, merge_bb
                scope.enter()
                
                irbuilder.setInsertPoint body_bb
                @visit n.body
                irbuilder.createBr while_bb

                scope.leave()
                                
                irbuilder.setInsertPoint merge_bb
                merge_bb

        visitForIn: (n) ->
                insertBlock = irbuilder.getInsertBlock()
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
                                
                irbuilder.createBr forin_bb
                irbuilder.setInsertPoint forin_bb

                moreleft = @createCall @ejs_runtime.prop_iterator_next, [iterator, (boolConstant true)], "moreleft"
                cmp = irbuilder.createICmpEq moreleft, (boolConstant false), "cmpmoreleft"
                irbuilder.createCondBr cmp, merge_bb, body_bb
                
                irbuilder.setInsertPoint body_bb
                current = @createCall @ejs_runtime.prop_iterator_current, [iterator], "iterator_current"
                @storeValueInDest current, lhs
                @visit n.body
                irbuilder.createBr forin_bb

                irbuilder.setInsertPoint merge_bb
                merge_bb
                
                
        visitUpdateExpression: (n) ->
                result = @createAlloca @currentFunction, EjsValueType, "%update_result"
                argument = @visit n.argument
                
                one = irbuilder.createLoad @ejs_runtime['one'], "load_one"
                
                if not n.prefix
                        # postfix updates store the argument before the op
                        irbuilder.createStore argument, result

                # argument = argument $op 1
                temp = @createCall @ejs_runtime["binop#{if n.operator is '++' then '+' else '-'}"], [argument, one], "update_temp"
                
                @storeValueInDest temp, n.argument
                
                # return result
                if n.prefix
                        argument = @visit n.argument
                        # prefix updates store the argument after the op
                        irbuilder.createStore argument, result
                irbuilder.createLoad result, "%update_result_load"

        visitConditionalExpression: (n) ->
                @visitIfOrCondExp n, true
                        
        visitIf: (n) ->
                @visitIfOrCondExp n, false

        visitIfOrCondExp: (n, load_result) ->

                if load_result
                        cond_val = @createAlloca @currentFunction, EjsValueType, "%cond_val"
                
                # first we convert our conditional EJSValue to a boolean
                cond_truthy = @createCall @ejs_runtime.truthy, [@visit(n.test)], "cond_truthy"

                insertBlock = irbuilder.getInsertBlock()
                insertFunc = insertBlock.parent

                then_bb  = new llvm.BasicBlock "then", insertFunc
                else_bb  = new llvm.BasicBlock "else", insertFunc
                merge_bb = new llvm.BasicBlock "merge", insertFunc

                # we invert the test here - check if the condition is false/0
                cmp = irbuilder.createICmpEq cond_truthy, (boolConstant false), "cmpresult"
                irbuilder.createCondBr cmp, else_bb, then_bb

                irbuilder.setInsertPoint then_bb
                then_val = @visit n.consequent
                if load_result
                        irbuilder.createStore then_val, cond_val
                irbuilder.createBr merge_bb

                irbuilder.setInsertPoint else_bb
                else_val = @visit n.alternate
                if load_result
                        irbuilder.createStore else_val, cond_val
                irbuilder.createBr merge_bb

                irbuilder.setInsertPoint merge_bb
                if load_result
                        irbuilder.createLoad cond_val, "cond_val_load"
                else
                        merge_bb
                
        visitReturn: (n) ->
                debug.log "visitReturn"
                if @iifeStack[0].iife_rv?
                        if n.argument?
                                irbuilder.createStore (@visit n.argument), @iifeStack[0].iife_rv
                        else
                                irbuilder.createStore @loadUndefinedEjsValue(), @iifeStack[0].iife_rv
                        irbuilder.createBr @iifeStack[0].iife_dest_bb
                else
                        rv = if n.argument? then (@visit n.argument) else @loadUndefinedEjsValue()
                        
                        if @finallyStack.length > 0
                                if not @returnValueAlloca?
                                        @returnValueAlloca = @createAlloca @currentFunction, EjsValueType, "returnValue"
                                irbuilder.createStore rv, @returnValueAlloca
                                irbuilder.createStore (int32Constant ExitableScope.REASON_RETURN), @currentFunction.cleanup_alloca
                                irbuilder.createBr @finallyStack[0]
                        else
                                return_alloca = @createAlloca @currentFunction, EjsValueType, "return_alloca"
                                irbuilder.createStore rv, return_alloca
                        
                                irbuilder.createRet irbuilder.createLoad return_alloca, "return_load"
                                                

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
                                                irbuilder.createStore initializer, allocas[i]
                                else
                                        initializer = @visitOrUndefined n.declarations[i].init
                                        irbuilder.createStore initializer, allocas[i]
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
                                                irbuilder.createStore initializer, allocas[i]
                                else
                                        initializer = @visitOrUndefined n.declarations[i].init
                                        irbuilder.createStore initializer, allocas[i]
                else if n.kind is "const"
                        for i in [0...n.declarations.length]
                                u = n.declarations[i]
                                initializer_ir = @visit u.init
                                # XXX bind the initializer to u.name in the current basic block and mark it as constant

        visitMemberExpression: (n) ->
                obj_result = @createAlloca @currentFunction, EjsValueType, "result_obj"
                obj_visit = @visit n.object
                irbuilder.createStore obj_visit, obj_result
                obj_load = irbuilder.createLoad obj_result, "obj_load"
                rv = @createPropertyLoad obj_load, n.property, n.computed
                load_result = @createAlloca @currentFunction, EjsValueType, "load_result"
                irbuilder.createStore rv, load_result
                if not n.result_not_used
                        irbuilder.createLoad load_result, "rv"

        storeValueInDest: (rhvalue, lhs) ->
                if lhs.type is syntax.Identifier
                        dest = @findIdentifierInScope lhs.name, @current_scope
                        if dest?
                                result = irbuilder.createStore rhvalue, dest
                        else
                                result = @createPropertyStore @loadGlobal(), lhs, rhvalue, false
                        result
                else if lhs.type is syntax.MemberExpression
                        object_alloca = @createAlloca @currentFunction, EjsValueType, "object_alloca"
                        irbuilder.createStore (@visit lhs.object), object_alloca
                        result = @createPropertyStore (irbuilder.createLoad object_alloca, "load_object"), lhs.property, rhvalue, lhs.computed
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
                insertBlock = irbuilder.getInsertBlock()

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
                irbuilder.setInsertPoint entry_bb

                new_scope = {}

                # we save off the top scope and entry_bb of the function so that we can hoist vars there
                ir_func.topScope = new_scope
                ir_func.entry_bb = entry_bb

                ir_func.literalAllocas = {}

                allocas = []

                # create allocas for the builtin args
                for i in [0...BUILTIN_PARAMS.length]
                        alloca = irbuilder.createAlloca BUILTIN_PARAMS[i].llvm_type, "local_#{n.params[i].name}"
                        new_scope[n.params[i].name] = alloca
                        allocas.push alloca

                # create an alloca to store our 'EJSValue** args' parameter, so we can pull the formal parameters out of it
                args_alloca = irbuilder.createAlloca EjsValueType.pointerTo(), "local_%args"
                new_scope["%args"] = args_alloca
                allocas.push args_alloca

                # now create allocas for the formal parameters
                for param in n.params[BUILTIN_PARAMS.length..]
                        if param.type is syntax.Identifier
                                alloca = @createAlloca @currentFunction, EjsValueType, "local_#{param.name}"
                                new_scope[param.name] = alloca
                                allocas.push alloca
                        else
                                debug.log "we don't handle destructured args at the moment."
                                console.warn JSON.stringify param
                                throw "we don't handle destructured args at the moment."

                debug.log "alloca #{alloca}" for alloca in allocas
        
                # now store the arguments (use .. to include our args array) onto the stack
                for i in [0..BUILTIN_PARAMS.length]
                        store = irbuilder.createStore ir_args[i], allocas[i]
                        debug.log "store #{store} *builtin"

                # initialize all our named parameters to undefined
                args_load = irbuilder.createLoad args_alloca, "args_load"
                if n.params.length > BUILTIN_PARAMS.length
                        for i in [BUILTIN_PARAMS.length...n.params.length]
                                store = irbuilder.createStore @loadUndefinedEjsValue(), allocas[i+1]
                        
                body_bb = new llvm.BasicBlock "body", ir_func
                irbuilder.setInsertPoint body_bb

                insertFunc = body_bb.parent
        
                # now pull the named parameters from our args array for the ones that were passed in.
                # any arg that isn't specified
                if n.params.length > BUILTIN_PARAMS.length
                        load_argc = irbuilder.createLoad allocas[2], "argc" # FIXME, magic number alert
                
                        for i in [BUILTIN_PARAMS.length...n.params.length]
                                then_bb  = new llvm.BasicBlock "arg_then", insertFunc
                                else_bb  = new llvm.BasicBlock "arg_else", insertFunc
                                merge_bb = new llvm.BasicBlock "arg_merge", insertFunc

                                cmp = irbuilder.createICmpSGt load_argc, (int32Constant i-BUILTIN_PARAMS.length), "argcmpresult"
                                irbuilder.createCondBr cmp, then_bb, else_bb
                        
                                irbuilder.setInsertPoint then_bb
                                arg_ptr = irbuilder.createGetElementPointer args_load, [(int32Constant i-BUILTIN_PARAMS.length)], "arg#{i-BUILTIN_PARAMS.length}_ptr"
                                debug.log "arg_ptr = #{arg_ptr}"
                                arg = irbuilder.createLoad arg_ptr, "arg#{i-BUILTIN_PARAMS.length-1}_load"
                                store = irbuilder.createStore arg, allocas[i+1]
                                debug.log "store #{store}"
                                irbuilder.createBr merge_bb

                                irbuilder.setInsertPoint else_bb
                                irbuilder.createBr merge_bb

                                irbuilder.setInsertPoint merge_bb
                        
                @iifeStack = []

                @finallyStack = []
                
                @visitWithScope new_scope, [n.body]

                # XXX more needed here - this lacks all sorts of control flow stuff.
                # Finish off the function.
                irbuilder.createRet @loadUndefinedEjsValue()

                # insert an unconditional branch from entry_bb to body here, now that we're
                # sure we're not going to be inserting allocas into the entry_bb anymore.
                irbuilder.setInsertPoint entry_bb
                irbuilder.createBr body_bb

                @currentFunction = null

                irbuilder.setInsertPoint insertBlock

                return ir_func

        visitUnaryExpression: (n) ->
                debug.log "operator = '#{n.operator}'"

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
                debug.log "operator = '#{n.operator}'"
                builtin = "binop#{n.operator}"
                callee = @ejs_runtime[builtin]
                if not callee
                        throw "Internal error: unhandled binary operator '#{n.operator}'"

                left_alloca = @createAlloca @currentFunction, EjsValueType, "binop_left"
                left_visited = @visit n.left
                irbuilder.createStore left_visited, left_alloca
                
                right_alloca = @createAlloca @currentFunction, EjsValueType, "binop_right"
                right_visited = @visit n.right
                irbuilder.createStore right_visited, right_alloca

                if n.left.is_constant? and n.right.is_constant?
                        console.warn "we could totally evaluate this at compile time, yo"
                        

                if left_visited.literal? and right_visited.literal?
                        if typeof left_visited.literal.value is "number" and typeof right_visited.literal.value is "number"
                                if n.operator is "<"
                                        return @loadBoolEjsValue left_visited.literal.value < right_visited.literal.value
                                        
                # call the actual runtime binaryop method
                call = @createCall callee, [(irbuilder.createLoad left_alloca, "binop_left_load"), (irbuilder.createLoad right_alloca, "binop_right_load")], "result_#{builtin}"
                call

        visitLogicalExpression: (n) ->
                debug.log "operator = '#{n.operator}'"
                result = @createAlloca @currentFunction, EjsValueType, "result_#{n.operator}"

                left_visited = @visit n.left
                cond_truthy = @createCall @ejs_runtime.truthy, [left_visited], "cond_truthy"

                insertBlock = irbuilder.getInsertBlock()
                insertFunc = insertBlock.parent
        
                left_bb  = new llvm.BasicBlock "cond_left", insertFunc
                right_bb  = new llvm.BasicBlock "cond_right", insertFunc
                merge_bb = new llvm.BasicBlock "cond_merge", insertFunc

                # we invert the test here - check if the condition is false/0
                cmp = irbuilder.createICmpEq cond_truthy, (boolConstant false), "cmpresult"
                irbuilder.createCondBr cmp, right_bb, left_bb

                irbuilder.setInsertPoint left_bb
                # inside the else branch, left was truthy
                if n.operator is "||"
                        # for || we short circuit out here
                        irbuilder.createStore left_visited, result
                else if n.operator is "&&"
                        # for && we evaluate the second and store it
                        irbuilder.createStore (@visit n.right), result
                else
                        throw "Internal error 99.1"
                irbuilder.createBr merge_bb

                irbuilder.setInsertPoint right_bb
                # inside the then branch, left was falsy
                if n.operator is "||"
                        # for || we evaluate the second and store it
                        irbuilder.createStore (@visit n.right), result
                else if n.operator is "&&"
                        # for && we short circuit out here
                        irbuilder.createStore left_visited, result
                else
                        throw "Internal error 99.1"
                irbuilder.createBr merge_bb

                irbuilder.setInsertPoint merge_bb
                rv = irbuilder.createLoad result, "result_#{n.operator}_load"

                irbuilder.setInsertPoint merge_bb

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
                        argv.push int32Constant args.length-1    # %argc. -1 because we pulled out the first arg to send as the closure

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
                argv.push int32Constant args.length-1    # %argc. -1 because we pulled out the first arg to send as the closure

                if args.length > 1
                        argv.push @visitOrNull args[i] for i in [1...args.length]

                argv
                                                                
        visitCallExpression: (n) ->
                debug.log "visitCall #{JSON.stringify n}"
                debug.log "          arguments length = #{n.arguments.length}"
                debug.log "          arguments[#{i}] =  #{JSON.stringify n.arguments[i]}" for i in [0...n.arguments.length]

                intrinsicHandler = @ejs_intrinsics[n.callee.name.slice(1)]
                if not intrinsicHandler
                        throw "Internal error: callee should not be null in visitCallExpression (callee = #{n.callee.name}, arguments = #{n.arguments.length})"

                intrinsicHandler.call @, n
                
        visitNewExpression: (n) ->
                if n.callee.type isnt syntax.Identifier or n.callee.name[0] isnt '%'
                        throw "invalid ctor #{JSON.stringify n.callee}"

                if n.callee.name isnt "%invokeClosure"
                        throw "new expressions may only have a callee of %invokeClosure, callee = #{n.callee.name}"
                        
                intrinsicHandler = @ejs_intrinsics[n.callee.name.slice(1)]
                if not intrinsicHandler
                        throw "Internal error: ctor should not be null"

                intrinsicHandler.call @, n, true

        visitThisExpression: (n) ->
                debug.log "visitThisExpression"
                @createLoadThis()

        visitIdentifier: (n) ->
                debug.log "identifier #{n.name}"
                val = n.name
                source = @findIdentifierInScope val, @current_scope
                if source?
                        rv = irbuilder.createLoad source, "load_#{val}"
                        return rv

                # special handling of the arguments object here, so we
                # only initialize/create it if the function is
                # actually going to use it.
                if val is "arguments"
                        arguments_alloca = @createAlloca @currentFunction, EjsValueType, "local_arguments_object"
                        saved_insert_point = irbuilder.getInsertBlock()
                        irbuilder.setInsertPoint @currentFunction.entry_bb

                        load_argc = irbuilder.createLoad @currentFunction.topScope["%argc"], "argc_load"
                        load_args = irbuilder.createLoad @currentFunction.topScope["%args"], "args_load"

                        arguments_object = @createCall @ejs_runtime.arguments_new, [load_argc, load_args], "argstmp"
                        irbuilder.createStore arguments_object, arguments_alloca
                        @currentFunction.topScope["arguments"] = arguments_alloca

                        irbuilder.setInsertPoint saved_insert_point
                        rv = irbuilder.createLoad arguments_alloca, "load_arguments"
                        return rv

                rv = null
                debug.log "calling getFunction for #{val}"
                rv = @module.getFunction val

                if not rv
                        debug.log "Symbol '#{val}' not found in current scope"
                        rv = @createPropertyLoad @loadGlobal(), n, false, false

                debug.log "returning #{rv}"
                rv

        visitObjectExpression: (n) ->
                obj = @createCall @ejs_runtime.object_create, [@loadNullEjsValue()], "objtmp"
                for property in n.properties
                        val = @visit property.value
                        key = property.key
                        @createPropertyStore obj, key, val, false
                obj

        visitArrayExpression: (n) ->
                obj = @createCall @ejs_runtime.array_new, [(int32Constant n.elements.length)], "arrtmp"
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
                        debug.log "literal string: #{n.value}"

                        # check if it's an atom first of all
                        atom_name = "atom-#{n.value}"
                        if @ejs_runtime[atom_name]?
                                strload = irbuilder.createLoad @ejs_runtime[atom_name], "%str_atom_load"
                                strload.literal = n
                                return strload

                        # if it isn't an atom, make sure we only
                        # allocate it once per function entry
                        literal_key = "string-" + n.value
                        if not @currentFunction.literalAllocas[literal_key]?
                                # only create 1 instance of string
                                # literals used in a function, and
                                # allocate them in the entry block
                                insertBlock = irbuilder.getInsertBlock()

                                irbuilder.setInsertPoint @currentFunction.entry_bb
                                str_alloca = irbuilder.createAlloca EjsValueType, "str-alloca-#{n.value}"
                                c = irbuilder.createGlobalStringPtr n.value, "strconst"
                                irbuilder.createStore (@createCall @ejs_runtime.string_new_utf8, [c], "strtmp"), str_alloca
                                @currentFunction.literalAllocas[literal_key] = str_alloca
                                
                                irbuilder.setInsertPoint insertBlock
                                
                        str_alloca = @currentFunction.literalAllocas[literal_key]
                        strload = irbuilder.createLoad str_alloca, "%str_alloca"

                        strload.literal = n
                        debug.log "strload = #{strload}"
                        return strload


                # regular expression literals
                if typeof n.raw is "string" and n.raw[0] is '/'
                        debug.log "literal regexp: #{n.raw}"
                        c = irbuilder.createGlobalStringPtr n.raw, "strconst"
                        regexpcall = @createCall @ejs_runtime.regexp_new_utf8, [c], "regexptmp"
                        debug.log "regexpcall = #{regexpcall}"
                        return regexpcall

                # number literals
                if typeof n.value is "number"
                        debug.log "literal number: #{n.value}"
                        if n.value is 0
                                numload = irbuilder.createLoad @ejs_runtime['zero'], "load_zero"
                        else if n.value is 1
                                numload = irbuilder.createLoad @ejs_runtime['one'], "load_one"
                        else
                                literal_key = "num-" + n.value
                                if @currentFunction.literalAllocas[literal_key]
                                        num_alloca = @currentFunction.literalAllocas[literal_key]
                                else
                                        # only create 1 instance of num literals used in a function, and allocate them in the entry block
                                        insertBlock = irbuilder.getInsertBlock()

                                        irbuilder.setInsertPoint @currentFunction.entry_bb
                                        num_alloca = irbuilder.createAlloca EjsValueType, "num-alloca-#{n.value}"
                                        c = llvm.ConstantFP.getDouble n.value
                                        call = @createCall @ejs_runtime.number_new, [c], "numconst-#{n.value}"
                                        irbuilder.createStore call, num_alloca
                                        @currentFunction.literalAllocas[literal_key] = num_alloca
                                        
                                        irbuilder.setInsertPoint insertBlock
                                
                                numload = irbuilder.createLoad num_alloca, "%num_alloca"
                        numload.literal = n
                        debug.log "numload = #{numload}"
                        return numload

                # boolean literals
                if typeof n.value is "boolean"
                        debug.log "literal boolean: #{n.value}"
                        return @loadBoolEjsValue n.value

                throw "Internal error: unrecognized literal of type #{typeof n.value}"

        createCall: (callee, argv, callname, canThrow) ->
                # if we're inside a try block we have to use createInvoke, and pass two basic blocks:
                #   the normal block, which is basically this IR instruction's continuation
                #   the unwind block, where we land if the call throws an exception.
                #
                # for some builtins we know they won't throw, so we can skip this step and still use createCall.
                if TryExitableScope.unwindStack.length is 0 or callee.doesNotThrow or not canThrow
                        calltmp = irbuilder.createCall callee, argv, callname
                        calltmp.setDoesNotThrow() if callee.doesNotThrow
                        calltmp.setDoesNotAccessMemory() if callee.doesNotAccessMemory
                        calltmp.setOnlyReadsMemory() if not callee.doesNotAccessMemory and callee.onlyReadsMemory
                else
                        insertBlock = irbuilder.getInsertBlock()
                        insertFunc = insertBlock.parent
                        normal_block  = new llvm.BasicBlock "normal", insertFunc
                        calltmp = irbuilder.createInvoke callee, argv, normal_block, TryExitableScope.unwindStack[0].getUnwindBlock(), callname
                        calltmp.setDoesNotThrow() if callee.doesNotThrow
                        calltmp.setDoesNotAccessMemory() if callee.doesNotAccessMemory
                        calltmp.setOnlyReadsMemory() if not callee.doesNotAccessMemory and callee.onlyReadsMemory
                        # after we've made our call we need to change the insertion point to our continuation
                        irbuilder.setInsertPoint normal_block
                calltmp
        
        visitThrow: (n) ->
                arg = @visit n.argument
                @createCall @ejs_runtime.throw, [arg], "", true

        visitTry: (n) ->
                insertBlock = irbuilder.getInsertBlock()
                insertFunc = insertBlock.parent

                # the reason we ended up in the finally block
                if not @currentFunction.cleanup_alloca?
                        @currentFunction.cleanup_alloca = @createAlloca @currentFunction, int32Type, "cleanup_reason"
                if not @currentFunction.caught_result_storage?
                        @currentFunction.caught_result_storage = @createAlloca @currentFunction, EjsValueType, "caught_result_storage"
                if not @currentFunction.exception_storage?
                        @currentFunction.exception_storage = @createAlloca @currentFunction, int8PointerType, "exception_storage"
                # XXX these stores should be in the entry block too, right after the allocas
                #irbuilder.createStore (llvm.Constant.getAggregateZero ejsValueType), caught_result_storage
                irbuilder.createStore (llvm.Constant.getNull int8PointerType), @currentFunction.exception_storage

                # the merge bb where everything branches to after successfully leaving a catch/finally block
                merge_block = new llvm.BasicBlock "try_merge", insertFunc

                # if we have a finally clause, create finally_block
                if n.finalizer?
                        finally_block = new llvm.BasicBlock "finally_bb", insertFunc
                        branch_target = finally_block
                        @finallyStack.unshift finally_block
                else
                        branch_target = merge_block

                scope = new TryExitableScope @currentFunction.cleanup_alloca, branch_target, (-> new llvm.BasicBlock "exception", insertFunc)
                scope.enter()

                @visit n.block

                if n.finalizer?
                        @finallyStack.shift()
                
                # at the end of the try block branch to the cleanup block with REASON_FALLOFF
                scope.exitAft false

                if scope.unwind_block
                        # if we have a catch clause, create catch_block
                        if n.handlers.length > 0
                                catch_block = new llvm.BasicBlock "catch_bb", insertFunc

                                irbuilder.setInsertPoint catch_block
                                @visit n.handlers[0]
                                irbuilder.createBr branch_target # XXX this should probably be to the cleanup block?

                        # if we have a handler then branch to it unconditionally, otherwise resumeunwind?
                        irbuilder.setInsertPoint scope.unwind_block

                        clause_count = if catch_block? then 1 else 0
                        casted_personality = irbuilder.createPointerCast @ejs_runtime.personality, int8PointerType, "personality"
                        caught_result = irbuilder.createLandingPad EjsValueType, casted_personality, clause_count, "caught_result"
                        caught_result.setCleanup true
                
                        if catch_block?
                                caught_result.addClause @ejs_runtime.exception_typeinfo
                                # XXX more here.  store the exception, check if it's actually an ejs exception and branch to internal/external exception handlers
                                irbuilder.createBr catch_block
                        else if finally_block?
                                irbuilder.createBr finally_block

                        # Unwind Resume Block (calls _Unwind_Resume)
                        unwind_resume_block = new llvm.BasicBlock "unwind_resume", insertFunc
                        irbuilder.setInsertPoint unwind_resume_block
                        irbuilder.createResume irbuilder.createLoad @currentFunction.caught_result_storage, "load_caught_result"

                scope.leave()

                # Finally Block
                if n.finalizer?
                        irbuilder.setInsertPoint finally_block
                        @visit n.finalizer

                        cleanup_reason = irbuilder.createLoad @currentFunction.cleanup_alloca, "cleanup_reason_load"

                        if @returnValueAlloca?
                                return_tramp = new llvm.BasicBlock "return_tramp", insertFunc
                                irbuilder.setInsertPoint return_tramp
                                
                                if @finallyStack.length > 0
                                        irbuilder.createStore (int32Constant ExitableScope.REASON_RETURN), @currentFunction.cleanup_alloca
                                        irbuilder.createBr @finallyStack[0]
                                else
                                        rv = irbuilder.createLoad @returnValueAlloca, "rv"
                                        irbuilder.createRet rv
                        
                        irbuilder.setInsertPoint finally_block
                        switch_stmt = irbuilder.createSwitch cleanup_reason, merge_block, scope.destinations.length + 1
                        if @returnValueAlloca?
                                switch_stmt.addCase (int32Constant ExitableScope.REASON_RETURN), return_tramp
                        
                irbuilder.setInsertPoint merge_block

        handleInvokeClosureIntrinsic: (exp, ctor_context= false) ->
                if ctor_context
                        argv = @visitArgsForConstruct @ejs_runtime.invoke_closure, exp.arguments
                else
                        argv = @visitArgsForCall @ejs_runtime.invoke_closure, true, exp.arguments

                modified_argv = (argv[n] for n in [0...BUILTIN_PARAMS.length])

                if argv.length > BUILTIN_PARAMS.length
                        argv = argv.slice BUILTIN_PARAMS.length
                        argv.forEach (a,i) =>
                                gep = irbuilder.createGetElementPointer @currentFunction.scratch_area, [(int32Constant 0), (int64Constant i)], "arg_gep_#{i}"
                                store = irbuilder.createStore argv[i], gep, "argv[#{i}]-store"

                        argsCast = irbuilder.createGetElementPointer @currentFunction.scratch_area, [(int32Constant 0), (int64Constant 0)], "call_args_load"
                                                
                        modified_argv[BUILTIN_PARAMS.length] = argsCast

                else
                        modified_argv[BUILTIN_PARAMS.length] = llvm.Constant.getNull EjsValueType.pointerTo()
                                
                argv = modified_argv
                call_result = @createAlloca @currentFunction, EjsValueType, "call_result"

                if decompose_closure_on_invoke
                        insertBlock = irbuilder.getInsertBlock()
                        insertFunc = insertBlock.parent

                        # inline decomposition of the closure (with a fallback to the runtime invoke if there are bound args)
                        # and direct call
                        runtime_invoke_bb = new llvm.BasicBlock "runtime_invoke_bb", insertFunc
                        direct_invoke_bb = new llvm.BasicBlock "direct_invoke_bb", insertFunc
                        invoke_merge_bb = new llvm.BasicBlock "invoke_merge_bb", insertFunc

                        func_alloca = @createAlloca @currentFunction, EjsClosureFuncType, "direct_invoke_func"
                        env_alloca = @createAlloca @currentFunction, EjsClosureEnvType, "direct_invoke_env"
                        this_alloca = @createAlloca @currentFunction, EjsValueType, "direct_invoke_this"

                        # provide the default "this" for decompose_closure.  if one was bound it'll get overwritten
                        irbuilder.createStore argv[1], this_alloca
                        
                        decompose_args = [ argv[0], func_alloca, env_alloca, this_alloca ]
                        decompose_rv = @createCall @ejs_runtime.decompose_closure, decompose_args, "decompose_rv", false
                        cmp = irbuilder.createICmpEq decompose_rv, (boolConstant false), "cmpresult"
                        irbuilder.createCondBr cmp, runtime_invoke_bb, direct_invoke_bb

                        # if there were bound args we have to fall back to the runtime invoke method (since we can't
                        # guarantee enough room in our scratch area -- should we inline a check here or pass the length
                        # of the scratch area to decompose?  perhaps...FIXME)
                        # 
                        irbuilder.setInsertPoint runtime_invoke_bb
                        calltmp = @createCall @ejs_runtime.invoke_closure, argv, "calltmp"
                        store = irbuilder.createStore calltmp, call_result
                        irbuilder.createBr invoke_merge_bb

                        # in the successful case we modify our argv with the responses and directly invoke the closure func
                        irbuilder.setInsertPoint direct_invoke_bb
                        direct_args = [ (irbuilder.createLoad env_alloca, "env"), (irbuilder.createLoad this_alloca, "this"), argv[2], argv[3] ]
                        calltmp = @createCall (irbuilder.createLoad func_alloca, "func"), direct_args, "calltmp"
                        store = irbuilder.createStore calltmp, call_result
                        irbuilder.createBr invoke_merge_bb

                        irbuilder.setInsertPoint invoke_merge_bb
                else
                        calltmp = @createCall @ejs_runtime.invoke_closure, argv, "calltmp"
                        store = irbuilder.createStore calltmp, call_result
        
                if ctor_context
                        argv[1]
                else
                        irbuilder.createLoad call_result, "call_result_load"
                        
                        
                        
                
        handleMakeClosureIntrinsic: (exp, ctor_context= false) ->
                argv = @visitArgsForCall @ejs_runtime.make_closure, false, exp.arguments
                closure_result = @createAlloca @currentFunction, EjsValueType, "closure_result"
                calltmp = @createCall @ejs_runtime.make_closure, argv, "closure_tmp"
                store = irbuilder.createStore calltmp, closure_result
                irbuilder.createLoad closure_result, "closure_result_load"

        handleMakeAnonClosureIntrinsic: (exp, ctor_context= false) ->
                argv = @visitArgsForCall @ejs_runtime.make_anon_closure, false, exp.arguments
                closure_result = @createAlloca @currentFunction, EjsValueType, "closure_result"
                calltmp = @createCall @ejs_runtime.make_anon_closure, argv, "closure_tmp"
                store = irbuilder.createStore calltmp, closure_result
                irbuilder.createLoad closure_result, "closure_result_load"
                                
                
        handleCreateArgScratchAreaIntrinsic: (exp, ctor_context= false) ->
                argsArrayType = llvm.ArrayType.get EjsValueType, exp.arguments[0].value
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

                # set the types of all later arguments to be EjsValueType
                param.llvm_type = EjsValueType for param in n.params[BUILTIN_PARAMS.length..]

                # the LLVMIR func we allocate takes the proper EJSValue** parameter in the 4th spot instead of all the parameters
                n.ir_func = takes_builtins @module.getOrInsertFunction n.ir_name, EjsValueType, (param.llvm_type for param in BUILTIN_PARAMS).concat [EjsValueType.pointerTo()]

                # enable shadow stack map for gc roots
                #n.ir_func.setGC "shadow-stack"
                
                ir_args = n.ir_func.args
                (ir_args[i].setName n.params[i].name) for i in [0...BUILTIN_PARAMS.length]
                ir_args[BUILTIN_PARAMS.length].setName "%args"

                # we don't need to recurse here since we won't have nested functions at this point
                n
                        
insert_toplevel_func = (tree, filename) ->
        sanitize = (filename) ->
                if __ejs?
                        filename = filename.replace ".js", ""
                        filename = filename.replace ".", "_"
                        filename = filename.replace ",", "_"
                        filename = filename.replace "-", "_"
                        filename = filename.replace "/", "_"
                        filename = filename.replace "\\", "_"
                        filename
                else
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

exports.compile = (tree, base_output_filename, source_filename) ->

        #console.warn "compiling #{source_filename}"
        
        tree = insert_toplevel_func tree, base_output_filename

        debug.log -> escodegen.generate tree

        toplevel_name = tree.body[0].id.name
        
        #tree = desugar tree
        #debug.log -> escodegen.generate tree

        debug.log "before closure conversion"
        debug.log -> escodegen.generate tree
        
        tree = closure_conversion.convert tree, path.basename source_filename

        debug.log "after closure conversion"
        debug.log -> escodegen.generate tree

        module = new llvm.Module "compiled-#{base_output_filename}"

        module.toplevel_name = toplevel_name

        visitor = new AddFunctionsVisitor module
        tree = visitor.visit tree

        debug.log -> escodegen.generate tree

        visitor = new LLVMIRVisitor module, source_filename
        visitor.visit tree

        module
