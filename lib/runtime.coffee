llvm = require 'llvm'
types = require 'types'

takes_builtins = types.takes_builtins
does_not_throw = types.does_not_throw
does_not_access_memory = types.does_not_access_memory
only_reads_memory = types.only_reads_memory
returns_ejsval_bool = types.returns_ejsval_bool

runtime_interface =
        personality:           -> @abi.createExternalFunction @module, "__ejs_personality_v0",           types.int32, [types.int32, types.int32, types.int64, types.int8Pointer, types.int8Pointer]

        invoke_closure:        -> takes_builtins @abi.createExternalFunction @module, "_ejs_invoke_closure", types.EjsValue, [types.EjsValue, types.EjsValue, types.int32, types.EjsValue.pointerTo()]
        make_closure:          -> @abi.createExternalFunction @module, "_ejs_function_new", types.EjsValue, [types.EjsValue, types.EjsValue, types.getEjsClosureFunc(@abi)]
        make_anon_closure:     -> @abi.createExternalFunction @module, "_ejs_function_new_anon", types.EjsValue, [types.EjsValue, types.getEjsClosureFunc(@abi)]
        decompose_closure:     -> @abi.createExternalFunction @module, "_ejs_decompose_closure", types.bool, [types.EjsValue, types.getEjsClosureFunc(@abi).pointerTo(), types.EjsClosureEnv.pointerTo(), types.EjsValue.pointerTo()]

        make_closure_env:      -> @abi.createExternalFunction @module, "_ejs_closureenv_new", types.EjsValue, [types.int32]
        get_env_slot_val:      -> @abi.createExternalFunction @module, "_ejs_closureenv_get_slot", types.EjsValue, [types.EjsValue, types.int32]
        get_env_slot_ref:      -> @abi.createExternalFunction @module, "_ejs_closureenv_get_slot_ref", types.EjsValue.pointerTo(), [types.EjsValue, types.int32]
        
        object_create:         -> @abi.createExternalFunction @module, "_ejs_object_create",             types.EjsValue, [types.EjsValue]
        arguments_new:         -> does_not_throw @abi.createExternalFunction @module, "_ejs_arguments_new",             types.EjsValue, [types.int32, types.EjsValue.pointerTo()]
        array_new:             -> @abi.createExternalFunction @module, "_ejs_array_new",                 types.EjsValue, [types.int32, types.bool]
        array_new_copy:        -> @abi.createExternalFunction @module, "_ejs_array_new_copy",            types.EjsValue, [types.int32, types.EjsValue.pointerTo()]
        number_new:            -> does_not_throw does_not_access_memory @abi.createExternalFunction @module, "_ejs_number_new",                types.EjsValue, [types.double]
        string_new_utf8:       -> only_reads_memory does_not_throw @abi.createExternalFunction @module, "_ejs_string_new_utf8",           types.EjsValue, [types.string]
        regexp_new_utf8:       -> @abi.createExternalFunction @module, "_ejs_regexp_new_utf8",           types.EjsValue, [types.string, types.string]
        truthy:                -> does_not_throw does_not_access_memory @abi.createExternalFunction @module, "_ejs_truthy",                    types.bool, [types.EjsValue]
        object_setprop:        -> @abi.createExternalFunction @module, "_ejs_object_setprop",            types.EjsValue, [types.EjsValue, types.EjsValue, types.EjsValue]
        object_getprop:        -> only_reads_memory @abi.createExternalFunction @module, "_ejs_object_getprop",           types.EjsValue, [types.EjsValue, types.EjsValue]
        global_setprop:        -> @abi.createExternalFunction @module, "_ejs_global_setprop",            types.EjsValue, [types.EjsValue, types.EjsValue]
        global_getprop:        -> only_reads_memory @abi.createExternalFunction @module, "_ejs_global_getprop",           types.EjsValue, [types.EjsValue]

        object_define_value_prop: -> @abi.createExternalFunction @module, "_ejs_object_define_value_property",  types.bool, [types.EjsValue, types.EjsValue, types.EjsValue, types.int32];
        
        prop_iterator_new:     -> @abi.createExternalFunction @module, "_ejs_property_iterator_new",     types.EjsPropIterator, [types.EjsValue]
        prop_iterator_current: -> @abi.createExternalFunction @module, "_ejs_property_iterator_current", types.EjsValue, [types.EjsPropIterator]
        prop_iterator_next:    -> @abi.createExternalFunction @module, "_ejs_property_iterator_next",    types.bool, [types.EjsPropIterator, types.bool]
        prop_iterator_free:    -> @abi.createExternalFunction @module, "_ejs_property_iterator_free",    types.void, [types.EjsPropIterator]
        begin_catch:           -> @abi.createExternalFunction @module, "_ejs_begin_catch",               types.EjsValue, [types.int8Pointer]
        end_catch:             -> @abi.createExternalFunction @module, "_ejs_end_catch",                 types.EjsValue, []
        throw_nativeerror_utf8:-> @abi.createExternalFunction @module, "_ejs_throw_nativeerror_utf8",    types.void, [types.int32, types.string]
        throw:                 -> @abi.createExternalFunction @module, "_ejs_throw",                     types.void, [types.EjsValue]
        rethrow:               -> @abi.createExternalFunction @module, "_ejs_rethrow",                   types.void, [types.EjsValue]

        init_string_literal:   -> @abi.createExternalFunction @module, "_ejs_string_init_literal",       types.void, [types.string, types.EjsValue.pointerTo(), types.EjsPrimString.pointerTo(), types.jschar.pointerTo(), types.int32]

        typeof_is_object:      -> returns_ejsval_bool only_reads_memory @abi.createExternalFunction @module, "_ejs_op_typeof_is_object",       types.EjsValue, [types.EjsValue]
        typeof_is_function:    -> returns_ejsval_bool only_reads_memory @abi.createExternalFunction @module, "_ejs_op_typeof_is_function",     types.EjsValue, [types.EjsValue]
        typeof_is_string:      -> returns_ejsval_bool only_reads_memory @abi.createExternalFunction @module, "_ejs_op_typeof_is_string",       types.EjsValue, [types.EjsValue]
        typeof_is_number:      -> returns_ejsval_bool only_reads_memory @abi.createExternalFunction @module, "_ejs_op_typeof_is_number",       types.EjsValue, [types.EjsValue]
        typeof_is_undefined:   -> returns_ejsval_bool only_reads_memory @abi.createExternalFunction @module, "_ejs_op_typeof_is_undefined",    types.EjsValue, [types.EjsValue]
        typeof_is_null:        -> returns_ejsval_bool only_reads_memory @abi.createExternalFunction @module, "_ejs_op_typeof_is_null",         types.EjsValue, [types.EjsValue]
        typeof_is_boolean:     -> returns_ejsval_bool only_reads_memory @abi.createExternalFunction @module, "_ejs_op_typeof_is_boolean",      types.EjsValue, [types.EjsValue]
        
        undefined:             -> @module.getOrInsertGlobal           "_ejs_undefined",                 types.EjsValue
        "true":                -> @module.getOrInsertGlobal           "_ejs_true",                      types.EjsValue
        "false":               -> @module.getOrInsertGlobal           "_ejs_false",                     types.EjsValue
        "null":                -> @module.getOrInsertGlobal           "_ejs_null",                      types.EjsValue
        "one":                 -> @module.getOrInsertGlobal           "_ejs_one",                       types.EjsValue
        "zero":                -> @module.getOrInsertGlobal           "_ejs_zero",                      types.EjsValue
        global:                -> @module.getOrInsertGlobal           "_ejs_global",                    types.EjsValue
        exception_typeinfo:    -> @module.getOrInsertGlobal           "EJS_EHTYPE_ejsvalue",            types.EjsExceptionTypeInfo
        function_specops:      -> @module.getOrInsertGlobal           "_ejs_function_specops",          types.EjsSpecops

        "unop-":           -> @abi.createExternalFunction @module, "_ejs_op_neg",         types.EjsValue, [types.EjsValue]
        "unop+":           -> @abi.createExternalFunction @module, "_ejs_op_plus",        types.EjsValue, [types.EjsValue]
        "unop!":           -> returns_ejsval_bool @abi.createExternalFunction @module, "_ejs_op_not",         types.EjsValue, [types.EjsValue]
        "unop~":           -> @abi.createExternalFunction @module, "_ejs_op_bitwise_not", types.EjsValue, [types.EjsValue]
        "unoptypeof":      -> does_not_throw @abi.createExternalFunction @module, "_ejs_op_typeof",      types.EjsValue, [types.EjsValue]
        "unopdelete":      -> @abi.createExternalFunction @module, "_ejs_op_delete",      types.EjsValue, [types.EjsValue, types.EjsValue] # this is a unop, but ours only works for memberexpressions
        "unopvoid":        -> @abi.createExternalFunction @module, "_ejs_op_void",        types.EjsValue, [types.EjsValue]

        log:               -> @abi.createExternalFunction @module, "_ejs_logstr",            types.void, [types.string]
        record_binop:      -> @abi.createExternalFunction @module, "_ejs_record_binop",      types.void, [types.int32, types.string, types.EjsValue, types.EjsValue]
        record_assignment: -> @abi.createExternalFunction @module, "_ejs_record_assignment", types.void, [types.int32, types.EjsValue]
        record_getprop:    -> @abi.createExternalFunction @module, "_ejs_record_getprop",    types.void, [types.int32, types.EjsValue, types.EjsValue]
        record_setprop:    -> @abi.createExternalFunction @module, "_ejs_record_setprop",    types.void, [types.int32, types.EjsValue, types.EjsValue, types.EjsValue]

exports.createInterface = (module, abi) ->
        runtime =
                module: module
                abi: abi
        for k of runtime_interface
                Object.defineProperty runtime, k, { get: runtime_interface[k] }
        runtime

exports.createBinopsInterface = (module, abi) ->
        return Object.create null, {
                "^":          { get: -> abi.createExternalFunction module, "_ejs_op_bitwise_xor", types.EjsValue, [types.EjsValue, types.EjsValue] }
                "&":          { get: -> abi.createExternalFunction module, "_ejs_op_bitwise_and", types.EjsValue, [types.EjsValue, types.EjsValue] }
                "|":          { get: -> abi.createExternalFunction module, "_ejs_op_bitwise_or",  types.EjsValue, [types.EjsValue, types.EjsValue] }
                ">>":         { get: -> abi.createExternalFunction module, "_ejs_op_rsh",         types.EjsValue, [types.EjsValue, types.EjsValue] }
                "<<":         { get: -> abi.createExternalFunction module, "_ejs_op_lsh",         types.EjsValue, [types.EjsValue, types.EjsValue] }
                ">>>":        { get: -> abi.createExternalFunction module, "_ejs_op_ursh",        types.EjsValue, [types.EjsValue, types.EjsValue] }
                "<<<":        { get: -> abi.createExternalFunction module, "_ejs_op_ulsh",        types.EjsValue, [types.EjsValue, types.EjsValue] }
                "%":          { get: -> abi.createExternalFunction module, "_ejs_op_mod",         types.EjsValue, [types.EjsValue, types.EjsValue] }
                "+":          { get: -> only_reads_memory abi.createExternalFunction module, "_ejs_op_add",         types.EjsValue, [types.EjsValue, types.EjsValue] }
                "*":          { get: -> abi.createExternalFunction module, "_ejs_op_mult",        types.EjsValue, [types.EjsValue, types.EjsValue] }
                "/":          { get: -> abi.createExternalFunction module, "_ejs_op_div",         types.EjsValue, [types.EjsValue, types.EjsValue] }
                "<":          { get: -> returns_ejsval_bool only_reads_memory abi.createExternalFunction module, "_ejs_op_lt",          types.EjsValue, [types.EjsValue, types.EjsValue] }
                "<=":         { get: -> returns_ejsval_bool abi.createExternalFunction module, "_ejs_op_le",          types.EjsValue, [types.EjsValue, types.EjsValue] }
                ">":          { get: -> returns_ejsval_bool abi.createExternalFunction module, "_ejs_op_gt",          types.EjsValue, [types.EjsValue, types.EjsValue] }
                ">=":         { get: -> returns_ejsval_bool abi.createExternalFunction module, "_ejs_op_ge",          types.EjsValue, [types.EjsValue, types.EjsValue] }
                "-":          { get: -> abi.createExternalFunction module, "_ejs_op_sub",         types.EjsValue, [types.EjsValue, types.EjsValue] }
                "===":        { get: -> returns_ejsval_bool does_not_throw does_not_access_memory abi.createExternalFunction module, "_ejs_op_strict_eq",   types.EjsValue, [types.EjsValue, types.EjsValue] }
                "==":         { get: -> returns_ejsval_bool abi.createExternalFunction module, "_ejs_op_eq",          types.EjsValue, [types.EjsValue, types.EjsValue] }
                "!==":        { get: -> returns_ejsval_bool does_not_throw does_not_access_memory abi.createExternalFunction module, "_ejs_op_strict_neq",  types.EjsValue, [types.EjsValue, types.EjsValue] }
                "!=":         { get: -> returns_ejsval_bool abi.createExternalFunction module, "_ejs_op_neq",         types.EjsValue, [types.EjsValue, types.EjsValue] }
                "instanceof": { get: -> returns_ejsval_bool abi.createExternalFunction module, "_ejs_op_instanceof",  types.EjsValue, [types.EjsValue, types.EjsValue] }
                "in":         { get: -> returns_ejsval_bool abi.createExternalFunction module, "_ejs_op_in",          types.EjsValue, [types.EjsValue, types.EjsValue] }
        }                
        
exports.createAtomsInterface = (module) ->
        return Object.create null, {
                "null":      { get: -> module.getOrInsertGlobal           "_ejs_atom_null",                 types.EjsValue }
                "undefined": { get: -> module.getOrInsertGlobal           "_ejs_atom_undefined",            types.EjsValue }
                "length":    { get: -> module.getOrInsertGlobal           "_ejs_atom_length",               types.EjsValue }
                "__ejs":     { get: -> module.getOrInsertGlobal           "_ejs_atom___ejs",                types.EjsValue }
                "object":    { get: -> module.getOrInsertGlobal           "_ejs_atom_object",               types.EjsValue }
                "function":  { get: -> module.getOrInsertGlobal           "_ejs_atom_function",             types.EjsValue }
                "prototype": { get: -> module.getOrInsertGlobal           "_ejs_atom_prototype",            types.EjsValue }
                "Object":    { get: -> module.getOrInsertGlobal           "_ejs_atom_Object",               types.EjsValue }
                "Array":     { get: -> module.getOrInsertGlobal           "_ejs_atom_Array",                types.EjsValue }
        }
        