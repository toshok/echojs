llvm = require 'llvm'
types = require 'types'

runtime_interface =
        personality:           -> @module.getOrInsertExternalFunction "__ejs_personality_v0",           types.int32, [types.int32, types.int32, types.int64, types.int8Pointer, types.int8Pointer]

        invoke_closure:        -> types.takes_builtins @module.getOrInsertExternalFunction "_ejs_invoke_closure", types.EjsValue, [types.EjsValue, types.EjsValue, types.int32, types.EjsValue.pointerTo()]
        make_closure:          -> @module.getOrInsertExternalFunction "_ejs_function_new", types.EjsValue, [types.EjsValue, types.EjsValue, types.EjsClosureFunc]
        make_anon_closure:     -> @module.getOrInsertExternalFunction "_ejs_function_new_anon", types.EjsValue, [types.EjsValue, types.EjsClosureFunc]
        decompose_closure:     -> @module.getOrInsertExternalFunction "_ejs_decompose_closure", types.bool, [types.EjsValue, types.EjsClosureFunc.pointerTo(), types.EjsClosureEnv.pointerTo(), types.EjsValue.pointerTo()]

        make_closure_env:      -> @module.getOrInsertExternalFunction "_ejs_closureenv_new", types.EjsValue, [types.int32]
        get_env_slot_val:      -> @module.getOrInsertExternalFunction "_ejs_closureenv_get_slot", types.EjsValue, [types.EjsValue, types.int32]
        get_env_slot_ref:      -> @module.getOrInsertExternalFunction "_ejs_closureenv_get_slot_ref", types.EjsValue.pointerTo(), [types.EjsValue, types.int32]
        
        object_create:         -> @module.getOrInsertExternalFunction "_ejs_object_create",             types.EjsValue, [types.EjsValue]
        arguments_new:         -> types.does_not_throw @module.getOrInsertExternalFunction "_ejs_arguments_new",             types.EjsValue, [types.int32, types.EjsValue.pointerTo()]
        array_new:             -> @module.getOrInsertExternalFunction "_ejs_array_new",                 types.EjsValue, [types.int32, types.bool]
        number_new:            -> types.does_not_throw types.does_not_access_memory @module.getOrInsertExternalFunction "_ejs_number_new",                types.EjsValue, [types.double]
        string_new_utf8:       -> types.only_reads_memory types.does_not_throw @module.getOrInsertExternalFunction "_ejs_string_new_utf8",           types.EjsValue, [types.string]
        regexp_new_utf8:       -> @module.getOrInsertExternalFunction "_ejs_regexp_new_utf8",           types.EjsValue, [types.string, types.string]
        truthy:                -> types.does_not_throw types.does_not_access_memory @module.getOrInsertExternalFunction "_ejs_truthy",                    types.bool, [types.EjsValue]
        object_setprop:        -> @module.getOrInsertExternalFunction "_ejs_object_setprop",            types.EjsValue, [types.EjsValue, types.EjsValue, types.EjsValue]
        object_getprop:        -> types.only_reads_memory @module.getOrInsertExternalFunction "_ejs_object_getprop",           types.EjsValue, [types.EjsValue, types.EjsValue]
        global_setprop:        -> @module.getOrInsertExternalFunction "_ejs_global_setprop",            types.EjsValue, [types.EjsValue, types.EjsValue]
        global_getprop:        -> types.only_reads_memory @module.getOrInsertExternalFunction "_ejs_global_getprop",           types.EjsValue, [types.EjsValue]

        object_define_value_prop: -> @module.getOrInsertExternalFunction "_ejs_object_define_value_property",  types.bool, [types.EjsValue, types.EjsValue, types.EjsValue, types.int32];
        
        prop_iterator_new:     -> @module.getOrInsertExternalFunction "_ejs_property_iterator_new",     types.EjsPropIterator, [types.EjsValue]
        prop_iterator_current: -> @module.getOrInsertExternalFunction "_ejs_property_iterator_current", types.EjsValue, [types.EjsPropIterator]
        prop_iterator_next:    -> @module.getOrInsertExternalFunction "_ejs_property_iterator_next",    types.bool, [types.EjsPropIterator, types.bool]
        prop_iterator_free:    -> @module.getOrInsertExternalFunction "_ejs_property_iterator_free",    types.void, [types.EjsPropIterator]
        begin_catch:           -> @module.getOrInsertExternalFunction "_ejs_begin_catch",               types.EjsValue, [types.int8Pointer]
        end_catch:             -> @module.getOrInsertExternalFunction "_ejs_end_catch",                 types.EjsValue, []
        throw:                 -> @module.getOrInsertExternalFunction "_ejs_throw",                     types.void, [types.EjsValue]
        rethrow:               -> @module.getOrInsertExternalFunction "_ejs_rethrow",                   types.void, [types.EjsValue]

        init_string_literal:   -> @module.getOrInsertExternalFunction "_ejs_string_init_literal",       types.void, [types.string, types.EjsValue.pointerTo(), types.EjsPrimString.pointerTo(), types.jschar.pointerTo(), types.int32]

        typeof_is_object:      -> @module.getOrInsertExternalFunction "_ejs_op_typeof_is_object",       types.EjsValue, [types.EjsValue]
        typeof_is_function:    -> @module.getOrInsertExternalFunction "_ejs_op_typeof_is_function",     types.EjsValue, [types.EjsValue]
        typeof_is_string:      -> @module.getOrInsertExternalFunction "_ejs_op_typeof_is_string",       types.EjsValue, [types.EjsValue]
        typeof_is_number:      -> @module.getOrInsertExternalFunction "_ejs_op_typeof_is_number",       types.EjsValue, [types.EjsValue]
        typeof_is_undefined:   -> @module.getOrInsertExternalFunction "_ejs_op_typeof_is_undefined",    types.EjsValue, [types.EjsValue]
        typeof_is_null:        -> @module.getOrInsertExternalFunction "_ejs_op_typeof_is_null",         types.EjsValue, [types.EjsValue]
        typeof_is_boolean:     -> @module.getOrInsertExternalFunction "_ejs_op_typeof_is_boolean",      types.EjsValue, [types.EjsValue]
        
        undefined:             -> @module.getOrInsertGlobal           "_ejs_undefined",                 types.EjsValue
        "true":                -> @module.getOrInsertGlobal           "_ejs_true",                      types.EjsValue
        "false":               -> @module.getOrInsertGlobal           "_ejs_false",                     types.EjsValue
        "null":                -> @module.getOrInsertGlobal           "_ejs_null",                      types.EjsValue
        "one":                 -> @module.getOrInsertGlobal           "_ejs_one",                       types.EjsValue
        "zero":                -> @module.getOrInsertGlobal           "_ejs_zero",                      types.EjsValue
        "atom-null":           -> @module.getOrInsertGlobal           "_ejs_atom_null",                 types.EjsValue
        "atom-undefined":      -> @module.getOrInsertGlobal           "_ejs_atom_undefined",            types.EjsValue
        "atom-length":         -> @module.getOrInsertGlobal           "_ejs_atom_length",               types.EjsValue
        "atom-__ejs":          -> @module.getOrInsertGlobal           "_ejs_atom___ejs",                types.EjsValue
        "atom-object":         -> @module.getOrInsertGlobal           "_ejs_atom_object",               types.EjsValue
        "atom-function":       -> @module.getOrInsertGlobal           "_ejs_atom_function",             types.EjsValue
        "atom-prototype":      -> @module.getOrInsertGlobal           "_ejs_atom_prototype",            types.EjsValue
        "atom-Object":         -> @module.getOrInsertGlobal           "_ejs_atom_Object",               types.EjsValue
        "atom-Array":          -> @module.getOrInsertGlobal           "_ejs_atom_Array",                types.EjsValue
        global:                -> @module.getOrInsertGlobal           "_ejs_global",                    types.EjsValue
        exception_typeinfo:    -> @module.getOrInsertGlobal           "EJS_EHTYPE_ejsvalue",            types.EjsExceptionTypeInfo

        "unop-":           -> @module.getOrInsertExternalFunction "_ejs_op_neg",         types.EjsValue, [types.EjsValue]
        "unop+":           -> @module.getOrInsertExternalFunction "_ejs_op_plus",        types.EjsValue, [types.EjsValue]
        "unop!":           -> types.returns_ejsval_bool @module.getOrInsertExternalFunction "_ejs_op_not",         types.EjsValue, [types.EjsValue]
        "unop~":           -> @module.getOrInsertExternalFunction "_ejs_op_bitwise_not", types.EjsValue, [types.EjsValue]
        "unoptypeof":      -> types.does_not_throw @module.getOrInsertExternalFunction "_ejs_op_typeof",      types.EjsValue, [types.EjsValue]
        "unopdelete":      -> @module.getOrInsertExternalFunction "_ejs_op_delete",      types.EjsValue, [types.EjsValue, types.EjsValue] # this is a unop, but ours only works for memberexpressions
        "unopvoid":        -> @module.getOrInsertExternalFunction "_ejs_op_void",        types.EjsValue, [types.EjsValue]
        "binop^":          -> @module.getOrInsertExternalFunction "_ejs_op_bitwise_xor", types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop&":          -> @module.getOrInsertExternalFunction "_ejs_op_bitwise_and", types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop|":          -> @module.getOrInsertExternalFunction "_ejs_op_bitwise_or",  types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop>>":         -> @module.getOrInsertExternalFunction "_ejs_op_rsh",         types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop<<":         -> @module.getOrInsertExternalFunction "_ejs_op_lsh",         types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop>>>":        -> @module.getOrInsertExternalFunction "_ejs_op_ursh",        types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop<<<":        -> @module.getOrInsertExternalFunction "_ejs_op_ulsh",        types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop%":          -> @module.getOrInsertExternalFunction "_ejs_op_mod",         types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop+":          -> types.only_reads_memory @module.getOrInsertExternalFunction "_ejs_op_add",         types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop*":          -> @module.getOrInsertExternalFunction "_ejs_op_mult",        types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop/":          -> @module.getOrInsertExternalFunction "_ejs_op_div",         types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop<":          -> types.returns_ejsval_bool types.only_reads_memory @module.getOrInsertExternalFunction "_ejs_op_lt",          types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop<=":         -> types.returns_ejsval_bool @module.getOrInsertExternalFunction "_ejs_op_le",          types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop>":          -> types.returns_ejsval_bool @module.getOrInsertExternalFunction "_ejs_op_gt",          types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop>=":         -> types.returns_ejsval_bool @module.getOrInsertExternalFunction "_ejs_op_ge",          types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop-":          -> @module.getOrInsertExternalFunction "_ejs_op_sub",         types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop===":        -> types.returns_ejsval_bool types.does_not_throw types.does_not_access_memory @module.getOrInsertExternalFunction "_ejs_op_strict_eq",   types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop==":         -> types.returns_ejsval_bool @module.getOrInsertExternalFunction "_ejs_op_eq",          types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop!==":        -> types.returns_ejsval_bool types.does_not_throw types.does_not_access_memory @module.getOrInsertExternalFunction "_ejs_op_strict_neq",  types.EjsValue, [types.EjsValue, types.EjsValue]
        "binop!=":         -> types.returns_ejsval_bool @module.getOrInsertExternalFunction "_ejs_op_neq",         types.EjsValue, [types.EjsValue, types.EjsValue]
        "binopinstanceof": -> types.returns_ejsval_bool @module.getOrInsertExternalFunction "_ejs_op_instanceof",  types.EjsValue, [types.EjsValue, types.EjsValue]
        "binopin":         -> types.returns_ejsval_bool @module.getOrInsertExternalFunction "_ejs_op_in",          types.EjsValue, [types.EjsValue, types.EjsValue]

exports.createInterface = (module) ->
        runtime =
                module: module
        Object.defineProperty runtime, k, { get: runtime_interface[k] } for k of runtime_interface
        runtime
        