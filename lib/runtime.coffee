llvm = require 'llvm'
types = require 'types'

takes_builtins = types.takes_builtins
does_not_throw = types.does_not_throw
does_not_access_memory = types.does_not_access_memory
only_reads_memory = types.only_reads_memory
returns_ejsval_bool = types.returns_ejsval_bool

runtime_interface =
        personality:           -> @abi.createExternalFunction @module, "__ejs_personality_v0",           types.int32, [types.int32, types.int32, types.int64, types.int8Pointer, types.int8Pointer]

        module_get:            -> @abi.createExternalFunction @module, "_ejs_module_get", types.EjsValue, [types.EjsValue]
        module_import_batch:   -> @abi.createExternalFunction @module, "_ejs_module_import_batch", types.void, [types.EjsValue, types.EjsValue, types.EjsValue]

        invoke_closure:        -> takes_builtins @abi.createExternalFunction @module, "_ejs_invoke_closure", types.EjsValue, [types.EjsValue, types.EjsValue, types.int32, types.EjsValue.pointerTo()]
        make_closure:          -> @abi.createExternalFunction @module, "_ejs_function_new", types.EjsValue, [types.EjsValue, types.EjsValue, types.getEjsClosureFunc(@abi)]
        make_anon_closure:     -> @abi.createExternalFunction @module, "_ejs_function_new_anon", types.EjsValue, [types.EjsValue, types.getEjsClosureFunc(@abi)]

        make_closure_env:      -> @abi.createExternalFunction @module, "_ejs_closureenv_new", types.EjsValue, [types.int32]
        get_env_slot_val:      -> @abi.createExternalFunction @module, "_ejs_closureenv_get_slot", types.EjsValue, [types.EjsValue, types.int32]
        get_env_slot_ref:      -> @abi.createExternalFunction @module, "_ejs_closureenv_get_slot_ref", types.EjsValue.pointerTo(), [types.EjsValue, types.int32]
        
        object_create:         -> @abi.createExternalFunction @module, "_ejs_object_create",             types.EjsValue, [types.EjsValue]
        arguments_new:         -> does_not_throw @abi.createExternalFunction @module, "_ejs_arguments_new",             types.EjsValue, [types.int32, types.EjsValue.pointerTo()]
        array_new:             -> @abi.createExternalFunction @module, "_ejs_array_new",                 types.EjsValue, [types.int32, types.bool]
        array_new_copy:        -> @abi.createExternalFunction @module, "_ejs_array_new_copy",            types.EjsValue, [types.int32, types.EjsValue.pointerTo()]
        array_from_iterables:  -> @abi.createExternalFunction @module, "_ejs_array_from_iterables",      types.EjsValue, [types.int32, types.EjsValue.pointerTo()]
        number_new:            -> does_not_throw does_not_access_memory @abi.createExternalFunction @module, "_ejs_number_new",                types.EjsValue, [types.double]
        string_new_utf8:       -> only_reads_memory does_not_throw @abi.createExternalFunction @module, "_ejs_string_new_utf8",           types.EjsValue, [types.string]
        regexp_new_utf8:       -> @abi.createExternalFunction @module, "_ejs_regexp_new_utf8",           types.EjsValue, [types.string, types.string]
        truthy:                -> does_not_throw does_not_access_memory @abi.createExternalFunction @module, "_ejs_truthy",                    types.bool, [types.EjsValue]
        object_setprop:        -> @abi.createExternalFunction @module, "_ejs_object_setprop",            types.EjsValue, [types.EjsValue, types.EjsValue, types.EjsValue]
        object_getprop:        -> only_reads_memory @abi.createExternalFunction @module, "_ejs_object_getprop",           types.EjsValue, [types.EjsValue, types.EjsValue]
        global_setprop:        -> @abi.createExternalFunction @module, "_ejs_global_setprop",            types.EjsValue, [types.EjsValue, types.EjsValue]
        global_getprop:        -> only_reads_memory @abi.createExternalFunction @module, "_ejs_global_getprop",           types.EjsValue, [types.EjsValue]

        object_define_accessor_prop: -> @abi.createExternalFunction @module, "_ejs_object_define_accessor_property",  types.bool, [types.EjsValue, types.EjsValue, types.EjsValue, types.EjsValue, types.int32];
        object_define_value_prop: -> @abi.createExternalFunction @module, "_ejs_object_define_value_property",  types.bool, [types.EjsValue, types.EjsValue, types.EjsValue, types.int32];

        object_set_prototype_of: -> @abi.createExternalFunction @module, "_ejs_object_set_prototype_of", types.EjsValue, [types.EjsValue, types.EjsValue];
        object_create_wrapper:   -> @abi.createExternalFunction @module, "_ejs_object_create_wrapper",     types.EjsValue, [types.EjsValue];
        prop_iterator_new:     -> @abi.createExternalFunction @module, "_ejs_property_iterator_new",     types.EjsPropIterator, [types.EjsValue]
        prop_iterator_current: -> @abi.createExternalFunction @module, "_ejs_property_iterator_current", types.EjsValue, [types.EjsPropIterator]
        prop_iterator_next:    -> @abi.createExternalFunction @module, "_ejs_property_iterator_next",    types.bool, [types.EjsPropIterator, types.bool]
        prop_iterator_free:    -> @abi.createExternalFunction @module, "_ejs_property_iterator_free",    types.void, [types.EjsPropIterator]
        begin_catch:           -> @abi.createExternalFunction @module, "_ejs_begin_catch",               types.EjsValue, [types.int8Pointer]
        end_catch:             -> @abi.createExternalFunction @module, "_ejs_end_catch",                 types.EjsValue, []
        throw_nativeerror_utf8:-> @abi.createExternalFunction @module, "_ejs_throw_nativeerror_utf8",    types.void, [types.int32, types.string]
        throw:                 -> @abi.createExternalFunction @module, "_ejs_throw",                     types.void, [types.EjsValue]
        rethrow:               -> @abi.createExternalFunction @module, "_ejs_rethrow",                   types.void, [types.EjsValue]

        ToString:              -> @abi.createExternalFunction @module, "ToString",                       types.EjsValue, [types.EjsValue]
        string_concat:         -> @abi.createExternalFunction @module, "_ejs_string_concat",             types.EjsValue, [types.EjsValue, types.EjsValue]
        init_string_literal:   -> @abi.createExternalFunction @module, "_ejs_string_init_literal",       types.void, [types.string, types.EjsValue.pointerTo(), types.EjsPrimString.pointerTo(), types.jschar.pointerTo(), types.int32]

        gc_add_root:           -> @abi.createExternalFunction @module, "_ejs_gc_add_root",               types.void, [types.EjsValue.pointerTo()]
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
        function_specops:      -> @module.getOrInsertGlobal           "_ejs_Function_specops",          types.EjsSpecops

        "unop-":           -> @abi.createExternalFunction @module, "_ejs_op_neg",         types.EjsValue, [types.EjsValue]
        "unop+":           -> @abi.createExternalFunction @module, "_ejs_op_plus",        types.EjsValue, [types.EjsValue]
        "unop!":           -> returns_ejsval_bool @abi.createExternalFunction @module, "_ejs_op_not",         types.EjsValue, [types.EjsValue]
        "unop~":           -> @abi.createExternalFunction @module, "_ejs_op_bitwise_not", types.EjsValue, [types.EjsValue]
        "unoptypeof":      -> does_not_throw @abi.createExternalFunction @module, "_ejs_op_typeof",      types.EjsValue, [types.EjsValue]
        "unopdelete":      -> @abi.createExternalFunction @module, "_ejs_op_delete",      types.EjsValue, [types.EjsValue, types.EjsValue] # this is a unop, but ours only works for memberexpressions
        "unopvoid":        -> @abi.createExternalFunction @module, "_ejs_op_void",        types.EjsValue, [types.EjsValue]

        dump_value:        -> @abi.createExternalFunction @module, "_ejs_dump_value",        types.void, [types.EjsValue]
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
        createBinop = (n) -> abi.createExternalFunction module, n, types.EjsValue, [types.EjsValue, types.EjsValue]
        return Object.create null, {
                "^":          { get: ->                     createBinop "_ejs_op_bitwise_xor" }
                "&":          { get: ->                     createBinop "_ejs_op_bitwise_and" }
                "|":          { get: ->                     createBinop "_ejs_op_bitwise_or" }
                ">>":         { get: ->                     createBinop "_ejs_op_rsh" }
                "<<":         { get: ->                     createBinop "_ejs_op_lsh" }
                ">>>":        { get: ->                     createBinop "_ejs_op_ursh" }
                "<<<":        { get: ->                     createBinop "_ejs_op_ulsh" }
                "%":          { get: ->                     createBinop "_ejs_op_mod" }
                "+":          { get: ->                     createBinop "_ejs_op_add" }
                "*":          { get: ->                     createBinop "_ejs_op_mult" }
                "/":          { get: ->                     createBinop "_ejs_op_div" }
                "-":          { get: ->                     createBinop "_ejs_op_sub" }
                "<":          { get: -> returns_ejsval_bool createBinop "_ejs_op_lt" }
                "<=":         { get: -> returns_ejsval_bool createBinop "_ejs_op_le" }
                ">":          { get: -> returns_ejsval_bool createBinop "_ejs_op_gt" }
                ">=":         { get: -> returns_ejsval_bool createBinop "_ejs_op_ge" }
                "===":        { get: -> returns_ejsval_bool createBinop "_ejs_op_strict_eq" }
                "==":         { get: -> returns_ejsval_bool createBinop "_ejs_op_eq" }
                "!==":        { get: -> returns_ejsval_bool createBinop "_ejs_op_strict_neq" }
                "!=":         { get: -> returns_ejsval_bool createBinop "_ejs_op_neq" }
                "instanceof": { get: -> returns_ejsval_bool createBinop "_ejs_op_instanceof" }
                "in":         { get: -> returns_ejsval_bool createBinop "_ejs_op_in" }
        }                
        
exports.createAtomsInterface = (module) ->
        getGlobal = (n) -> module.getOrInsertGlobal n, types.EjsValue
        return Object.create null, {
                "null":      { get: -> getGlobal "_ejs_atom_null" }
                "undefined": { get: -> getGlobal "_ejs_atom_undefined" }
                "length":    { get: -> getGlobal "_ejs_atom_length" }
                "__ejs":     { get: -> getGlobal "_ejs_atom___ejs" }
                "object":    { get: -> getGlobal "_ejs_atom_object" }
                "function":  { get: -> getGlobal "_ejs_atom_function" }
                "prototype": { get: -> getGlobal "_ejs_atom_prototype" }
                "Object":    { get: -> getGlobal "_ejs_atom_Object" }
                "Array":     { get: -> getGlobal "_ejs_atom_Array" }
        }

exports.createGlobalsInterface = (module) ->
        getGlobal = (n) -> module.getOrInsertGlobal n, types.EjsValue
        return Object.create null, {
                "Object":             { get: -> getGlobal "_ejs_Object" }
                "Object_prototype":   { get: -> getGlobal "_ejs_Object_prototype" }
                "Boolean":            { get: -> getGlobal "_ejs_Boolean" }
                "String":             { get: -> getGlobal "_ejs_String" }
                "Number":             { get: -> getGlobal "_ejs_Number" }
                "Array":              { get: -> getGlobal "_ejs_Array" }
                "DataView":           { get: -> getGlobal "_ejs_DataView" }
                "Date":               { get: -> getGlobal "_ejs_Date" }
                "Error":              { get: -> getGlobal "_ejs_Error" }
                "EvalError":          { get: -> getGlobal "_ejs_EvalError" }
                "RangeError":         { get: -> getGlobal "_ejs_RangeError" }
                "ReferenceError":     { get: -> getGlobal "_ejs_ReferenceError" }
                "SyntaxError":        { get: -> getGlobal "_ejs_SyntaxError" }
                "TypeError":          { get: -> getGlobal "_ejs_TypeError" }
                "URIError":           { get: -> getGlobal "_ejs_URIError" }
                "Function":           { get: -> getGlobal "_ejs_Function" }
                "JSON":               { get: -> getGlobal "_ejs_JSON" }
                "Math":               { get: -> getGlobal "_ejs_Math" }
                "Map":                { get: -> getGlobal "_ejs_Map" }
                "Proxy":              { get: -> getGlobal "_ejs_Proxy" }
                "Promise":            { get: -> getGlobal "_ejs_Promise" }
                "Reflect":            { get: -> getGlobal "_ejs_Reflect" }
                "RegExp":             { get: -> getGlobal "_ejs_RegExp" }
                "Symbol":             { get: -> getGlobal "_ejs_Symbol" }
                "Set":                { get: -> getGlobal "_ejs_Set" }
                "console":            { get: -> getGlobal "_ejs_console" }
                "ArrayBuffer":        { get: -> getGlobal "_ejs_ArrayBuffer" }
                "Int8Array":          { get: -> getGlobal "_ejs_Int8Array" }
                "Int16Array":         { get: -> getGlobal "_ejs_Int16Array" }
                "Int32Array":         { get: -> getGlobal "_ejs_Int32Array" }
                "Uint8Array":         { get: -> getGlobal "_ejs_Uint8Array" }
                "Uint16Array":        { get: -> getGlobal "_ejs_Uint16Array" }
                "Uint32Array":        { get: -> getGlobal "_ejs_Uint32Array" }
                "Float32Array":       { get: -> getGlobal "_ejs_Float32Array" }
                "Float64Array":       { get: -> getGlobal "_ejs_Float64Array" }
                "XMLHttpRequest":     { get: -> getGlobal "_ejs_XMLHttpRequest" }
                "process":            { get: -> getGlobal "_ejs_Process" }
                "require":            { get: -> getGlobal "_ejs_require" }
                "isNaN":              { get: -> getGlobal "_ejs_isNaN" }
                "isFinite":           { get: -> getGlobal "_ejs_isFinite" }
                "parseInt":           { get: -> getGlobal "_ejs_parseInt" }
                "parseFloat":         { get: -> getGlobal "_ejs_parseFloat" }
                "decodeURI":          { get: -> getGlobal "_ejs_decodeURI" }
                "encodeURI":          { get: -> getGlobal "_ejs_encodeURI" }
                "decodeURIComponent": { get: -> getGlobal "_ejs_decodeURIComponent" }
                "encodeURIComponent": { get: -> getGlobal "_ejs_encodeURIComponent" }
                "undefined":          { get: -> getGlobal "_ejs_undefined" }
                "Infinity":           { get: -> getGlobal "_ejs_Infinity" }
                "NaN":                { get: -> getGlobal "_ejs_nan" }
                "__ejs":              { get: -> getGlobal "_ejs__ejs" }
                # kind of a hack, but since we don't define these...
                "window":             { get: -> getGlobal "_ejs_undefined" }
                "document":           { get: -> getGlobal "_ejs_undefined" }
        }

exports.createSymbolsInterface = (module) ->
        getGlobal = (n) -> module.getOrInsertGlobal n, types.EjsValue
        return Object.create null, {
                "create":      { get: -> getGlobal "_ejs_Symbol_create" }
        }
