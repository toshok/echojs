/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import * as llvm from '@llvm';
import * as types from './types';

let takes_builtins = types.takes_builtins;
let does_not_throw = types.does_not_throw;
let does_not_access_memory = types.does_not_access_memory;
let only_reads_memory = types.only_reads_memory;
let returns_ejsval_bool = types.returns_ejsval_bool;

const runtime_interface = {
    personality:           function() { return this.abi.createExternalFunction(this.module, "__ejs_personality_v0",           types.Int32, [types.Int32, types.Int32, types.Int64, types.Int8Pointer, types.Int8Pointer]); },

    module_get:            function() { return this.abi.createExternalFunction(this.module, "_ejs_module_get", types.EjsValue, [types.EjsValue]); },
    module_import_batch:   function() { return this.abi.createExternalFunction(this.module, "_ejs_module_import_batch", types.Void, [types.EjsValue, types.EjsValue, types.EjsValue]); },

    invoke_closure:        function() { return takes_builtins(this.abi.createExternalFunction(this.module, "_ejs_invoke_closure", types.EjsValue, [types.EjsValue, types.EjsValue, types.Int32, types.EjsValue.pointerTo()])); },
    make_closure:          function() { return this.abi.createExternalFunction(this.module, "_ejs_function_new", types.EjsValue, [types.EjsValue, types.EjsValue, types.getEjsClosureFunc(this.abi)]); },
    make_anon_closure:     function() { return this.abi.createExternalFunction(this.module, "_ejs_function_new_anon", types.EjsValue, [types.EjsValue, types.getEjsClosureFunc(this.abi)]); },

    make_closure_env:      function() { return this.abi.createExternalFunction(this.module, "_ejs_closureenv_new", types.EjsValue, [types.Int32]); },
    get_env_slot_val:      function() { return this.abi.createExternalFunction(this.module, "_ejs_closureenv_get_slot", types.EjsValue, [types.EjsValue, types.Int32]); },
    get_env_slot_ref:      function() { return this.abi.createExternalFunction(this.module, "_ejs_closureenv_get_slot_ref", types.EjsValue.pointerTo(), [types.EjsValue, types.Int32]); },
    
    object_create:         function() { return this.abi.createExternalFunction(this.module, "_ejs_object_create",             types.EjsValue, [types.EjsValue]); },
    arguments_new:         function() { return does_not_throw(this.abi.createExternalFunction(this.module, "_ejs_arguments_new",             types.EjsValue, [types.Int32, types.EjsValue.pointerTo()])); },
    array_new:             function() { return this.abi.createExternalFunction(this.module, "_ejs_array_new",                 types.EjsValue, [types.Int32, types.Bool]); },
    array_new_copy:        function() { return this.abi.createExternalFunction(this.module, "_ejs_array_new_copy",            types.EjsValue, [types.Int32, types.EjsValue.pointerTo()]); },
    array_from_iterables:  function() { return this.abi.createExternalFunction(this.module, "_ejs_array_from_iterables",      types.EjsValue, [types.Int32, types.EjsValue.pointerTo()]); },
    number_new:            function() { return does_not_throw(does_not_access_memory(this.abi.createExternalFunction(this.module, "_ejs_number_new",                types.EjsValue, [types.Double]))); },
    string_new_utf8:       function() { return only_reads_memory(does_not_throw(this.abi.createExternalFunction(this.module, "_ejs_string_new_utf8",           types.EjsValue, [types.String]))); },
    regexp_new_utf8:       function() { return this.abi.createExternalFunction(this.module, "_ejs_regexp_new_utf8",           types.EjsValue, [types.String, types.String]); },
    truthy:                function() { return does_not_throw(does_not_access_memory(this.abi.createExternalFunction(this.module, "_ejs_truthy",                    types.Bool, [types.EjsValue]))); },
    object_setprop:        function() { return this.abi.createExternalFunction(this.module, "_ejs_object_setprop",            types.EjsValue, [types.EjsValue, types.EjsValue, types.EjsValue]); },
    object_getprop:        function() { return only_reads_memory(this.abi.createExternalFunction(this.module, "_ejs_object_getprop",           types.EjsValue, [types.EjsValue, types.EjsValue])); },
    global_setprop:        function() { return this.abi.createExternalFunction(this.module, "_ejs_global_setprop",            types.EjsValue, [types.EjsValue, types.EjsValue]); },
    global_getprop:        function() { return only_reads_memory(this.abi.createExternalFunction(this.module, "_ejs_global_getprop",           types.EjsValue, [types.EjsValue])); },

    object_define_accessor_prop: function() { return this.abi.createExternalFunction(this.module, "_ejs_object_define_accessor_property",  types.Bool, [types.EjsValue, types.EjsValue, types.EjsValue, types.EjsValue, types.Int32]); },
    object_define_value_prop: function() { return this.abi.createExternalFunction(this.module, "_ejs_object_define_value_property",  types.Bool, [types.EjsValue, types.EjsValue, types.EjsValue, types.Int32]); },

    object_set_prototype_of: function() { return this.abi.createExternalFunction(this.module, "_ejs_object_set_prototype_of", types.EjsValue, [types.EjsValue, types.EjsValue]); },
    object_create:         function() { return this.abi.createExternalFunction(this.module, "_ejs_object_create",             types.EjsValue, [types.EjsValue]); },
    prop_iterator_new:     function() { return this.abi.createExternalFunction(this.module, "_ejs_property_iterator_new",     types.EjsPropIterator, [types.EjsValue]); },
    prop_iterator_current: function() { return this.abi.createExternalFunction(this.module, "_ejs_property_iterator_current", types.EjsValue, [types.EjsPropIterator]); },
    prop_iterator_next:    function() { return this.abi.createExternalFunction(this.module, "_ejs_property_iterator_next",    types.Bool, [types.EjsPropIterator, types.Bool]); },
    prop_iterator_free:    function() { return this.abi.createExternalFunction(this.module, "_ejs_property_iterator_free",    types.Void, [types.EjsPropIterator]); },
    begin_catch:           function() { return this.abi.createExternalFunction(this.module, "_ejs_begin_catch",               types.EjsValue, [types.Int8Pointer]); },
    end_catch:             function() { return this.abi.createExternalFunction(this.module, "_ejs_end_catch",                 types.EjsValue, []); },
    throw_nativeerror_utf8:function() { return this.abi.createExternalFunction(this.module, "_ejs_throw_nativeerror_utf8",    types.Void, [types.Int32, types.String]); },
    throw:                 function() { return this.abi.createExternalFunction(this.module, "_ejs_throw",                     types.Void, [types.EjsValue]); },
    rethrow:               function() { return this.abi.createExternalFunction(this.module, "_ejs_rethrow",                   types.Void, [types.EjsValue]); },

    ToString:              function() { return this.abi.createExternalFunction(this.module, "ToString",                       types.EjsValue, [types.EjsValue]); },
    string_concat:         function() { return this.abi.createExternalFunction(this.module, "_ejs_string_concat",             types.EjsValue, [types.EjsValue, types.EjsValue]); },
    init_string_literal:   function() { return this.abi.createExternalFunction(this.module, "_ejs_string_init_literal",       types.Void, [types.String, types.EjsValue.pointerTo(), types.EjsPrimString.pointerTo(), types.JSChar.pointerTo(), types.Int32]); },

    gc_add_root:           function() { return this.abi.createExternalFunction(this.module, "_ejs_gc_add_root",               types.Void, [types.EjsValue.pointerTo()]); },
    typeof_is_object:      function() { return returns_ejsval_bool(only_reads_memory(this.abi.createExternalFunction(this.module, "_ejs_op_typeof_is_object",       types.EjsValue, [types.EjsValue]))); },
    typeof_is_function:    function() { return returns_ejsval_bool(only_reads_memory(this.abi.createExternalFunction(this.module, "_ejs_op_typeof_is_function",     types.EjsValue, [types.EjsValue]))); },
    typeof_is_string:      function() { return returns_ejsval_bool(only_reads_memory(this.abi.createExternalFunction(this.module, "_ejs_op_typeof_is_string",       types.EjsValue, [types.EjsValue]))); },
    typeof_is_number:      function() { return returns_ejsval_bool(only_reads_memory(this.abi.createExternalFunction(this.module, "_ejs_op_typeof_is_number",       types.EjsValue, [types.EjsValue]))); },
    typeof_is_undefined:   function() { return returns_ejsval_bool(only_reads_memory(this.abi.createExternalFunction(this.module, "_ejs_op_typeof_is_undefined",    types.EjsValue, [types.EjsValue]))); },
    typeof_is_null:        function() { return returns_ejsval_bool(only_reads_memory(this.abi.createExternalFunction(this.module, "_ejs_op_typeof_is_null",         types.EjsValue, [types.EjsValue]))); },
    typeof_is_boolean:     function() { return returns_ejsval_bool(only_reads_memory(this.abi.createExternalFunction(this.module, "_ejs_op_typeof_is_boolean",      types.EjsValue, [types.EjsValue]))); },
    
    undefined:             function() { return this.module.getOrInsertGlobal           ("_ejs_undefined",                 types.EjsValue); },
    "true":                function() { return this.module.getOrInsertGlobal           ("_ejs_true",                      types.EjsValue); },
    "false":               function() { return this.module.getOrInsertGlobal           ("_ejs_false",                     types.EjsValue); },
    "null":                function() { return this.module.getOrInsertGlobal           ("_ejs_null",                      types.EjsValue); },
    "one":                 function() { return this.module.getOrInsertGlobal           ("_ejs_one",                       types.EjsValue); },
    "zero":                function() { return this.module.getOrInsertGlobal           ("_ejs_zero",                      types.EjsValue); },
    global:                function() { return this.module.getOrInsertGlobal           ("_ejs_global",                    types.EjsValue); },
    exception_typeinfo:    function() { return this.module.getOrInsertGlobal           ("EJS_EHTYPE_ejsvalue",            types.EjsExceptionTypeInfo); },
    function_specops:      function() { return this.module.getOrInsertGlobal           ("_ejs_Function_specops",          types.EjsSpecops); },
    symbol_specops:        function() { return this.module.getOrInsertGlobal           ("_ejs_Symbol_specops",            types.EjsSpecops); },

    "unop-":           function() { return this.abi.createExternalFunction(this.module, "_ejs_op_neg",         types.EjsValue, [types.EjsValue]); },
    "unop+":           function() { return this.abi.createExternalFunction(this.module, "_ejs_op_plus",        types.EjsValue, [types.EjsValue]); },
    "unop!":           function() { return returns_ejsval_bool(this.abi.createExternalFunction(this.module, "_ejs_op_not",         types.EjsValue, [types.EjsValue])); },
    "unop~":           function() { return this.abi.createExternalFunction(this.module, "_ejs_op_bitwise_not", types.EjsValue, [types.EjsValue]); },
    "unoptypeof":      function() { return does_not_throw(this.abi.createExternalFunction(this.module, "_ejs_op_typeof",      types.EjsValue, [types.EjsValue])); },
    "unopdelete":      function() { return this.abi.createExternalFunction(this.module, "_ejs_op_delete",      types.EjsValue, [types.EjsValue, types.EjsValue]); }, // this is a unop, but ours only works for memberexpressions
    "unopvoid":        function() { return this.abi.createExternalFunction(this.module, "_ejs_op_void",        types.EjsValue, [types.EjsValue]); },

    dump_value:        function() { return this.abi.createExternalFunction(this.module, "_ejs_dump_value",        types.Void, [types.EjsValue]); },
    log:               function() { return this.abi.createExternalFunction(this.module, "_ejs_logstr",            types.Void, [types.String]); },
    record_binop:      function() { return this.abi.createExternalFunction(this.module, "_ejs_record_binop",      types.Void, [types.Int32, types.String, types.EjsValue, types.EjsValue]); },
    record_assignment: function() { return this.abi.createExternalFunction(this.module, "_ejs_record_assignment", types.Void, [types.Int32, types.EjsValue]); },
    record_getprop:    function() { return this.abi.createExternalFunction(this.module, "_ejs_record_getprop",    types.Void, [types.Int32, types.EjsValue, types.EjsValue]); },
    record_setprop:    function() { return this.abi.createExternalFunction(this.module, "_ejs_record_setprop",    types.Void, [types.Int32, types.EjsValue, types.EjsValue, types.EjsValue]); }
};

export function createInterface(module, abi) {
    let runtime = {
        module: module,
        abi: abi
    };

    for (let k of Object.keys(runtime_interface))
        Object.defineProperty(runtime, k, { get: runtime_interface[k] });
    return runtime;
}

export function createBinopsInterface(module, abi) {
    let createBinop = (n) => abi.createExternalFunction(module, n, types.EjsValue, [types.EjsValue, types.EjsValue]);
    return Object.create(null, {
        "^":          { get: () =>                     createBinop("_ejs_op_bitwise_xor") },
        "&":          { get: () =>                     createBinop("_ejs_op_bitwise_and") },
        "|":          { get: () =>                     createBinop("_ejs_op_bitwise_or") },
        ">>":         { get: () =>                     createBinop("_ejs_op_rsh") },
        "<<":         { get: () =>                     createBinop("_ejs_op_lsh") },
        ">>>":        { get: () =>                     createBinop("_ejs_op_ursh") },
        "<<<":        { get: () =>                     createBinop("_ejs_op_ulsh") },
        "%":          { get: () =>                     createBinop("_ejs_op_mod") },
        "+":          { get: () =>                     createBinop("_ejs_op_add") },
        "*":          { get: () =>                     createBinop("_ejs_op_mult") },
        "/":          { get: () =>                     createBinop("_ejs_op_div") },
        "-":          { get: () =>                     createBinop("_ejs_op_sub") },
        "<":          { get: () => returns_ejsval_bool(createBinop("_ejs_op_lt")) },
        "<=":         { get: () => returns_ejsval_bool(createBinop("_ejs_op_le")) },
        ">":          { get: () => returns_ejsval_bool(createBinop("_ejs_op_gt")) },
        ">=":         { get: () => returns_ejsval_bool(createBinop("_ejs_op_ge")) },
        "===":        { get: () => returns_ejsval_bool(createBinop("_ejs_op_strict_eq")) },
        "==":         { get: () => returns_ejsval_bool(createBinop("_ejs_op_eq")) },
        "!==":        { get: () => returns_ejsval_bool(createBinop("_ejs_op_strict_neq")) },
        "!=":         { get: () => returns_ejsval_bool(createBinop("_ejs_op_neq")) },
        "instanceof": { get: () => returns_ejsval_bool(createBinop("_ejs_op_instanceof")) },
        "in":         { get: () => returns_ejsval_bool(createBinop("_ejs_op_in")) }
    });
}

export function createAtomsInterface(module) {
    let getGlobal = (n) => module.getOrInsertGlobal(n, types.EjsValue);
    return Object.create (null, {
        "null":      { get: () => getGlobal("_ejs_atom_null") },
        "undefined": { get: () => getGlobal("_ejs_atom_undefined") },
        "length":    { get: () => getGlobal("_ejs_atom_length") },
        "__ejs":     { get: () => getGlobal("_ejs_atom___ejs") },
        "object":    { get: () => getGlobal("_ejs_atom_object") },
        "function":  { get: () => getGlobal("_ejs_atom_function") },
        "prototype": { get: () => getGlobal("_ejs_atom_prototype") },
        "Object":    { get: () => getGlobal("_ejs_atom_Object") },
        "Array":     { get: () => getGlobal("_ejs_atom_Array") }
    });
}


export function createGlobalsInterface(module) {
    let getGlobal = (n) => module.getOrInsertGlobal(n, types.EjsValue);
    return Object.create(null, {
        "Object":             { get: () => getGlobal("_ejs_Object") },
        "Object_prototype":   { get: () => getGlobal("_ejs_Object_prototype") },
        "Boolean":            { get: () => getGlobal("_ejs_Boolean") },
        "String":             { get: () => getGlobal("_ejs_String") },
        "Number":             { get: () => getGlobal("_ejs_Number") },
        "Array":              { get: () => getGlobal("_ejs_Array") },
        "DataView":           { get: () => getGlobal("_ejs_DataView") },
        "Date":               { get: () => getGlobal("_ejs_Date") },
        "Error":              { get: () => getGlobal("_ejs_Error") },
        "EvalError":          { get: () => getGlobal("_ejs_EvalError") },
        "RangeError":         { get: () => getGlobal("_ejs_RangeError") },
        "ReferenceError":     { get: () => getGlobal("_ejs_ReferenceError") },
        "SyntaxError":        { get: () => getGlobal("_ejs_SyntaxError") },
        "TypeError":          { get: () => getGlobal("_ejs_TypeError") },
        "URIError":           { get: () => getGlobal("_ejs_URIError") },
        "Function":           { get: () => getGlobal("_ejs_Function") },
        "JSON":               { get: () => getGlobal("_ejs_JSON") },
        "Math":               { get: () => getGlobal("_ejs_Math") },
        "Map":                { get: () => getGlobal("_ejs_Map") },
        "Proxy":              { get: () => getGlobal("_ejs_Proxy") },
        "Promise":            { get: () => getGlobal("_ejs_Promise") },
        "Reflect":            { get: () => getGlobal("_ejs_Reflect") },
        "RegExp":             { get: () => getGlobal("_ejs_RegExp") },
        "Symbol":             { get: () => getGlobal("_ejs_Symbol") },
        "Set":                { get: () => getGlobal("_ejs_Set") },
        "console":            { get: () => getGlobal("_ejs_console") },
        "ArrayBuffer":        { get: () => getGlobal("_ejs_ArrayBuffer") },
        "Int8Array":          { get: () => getGlobal("_ejs_Int8Array") },
        "Int16Array":         { get: () => getGlobal("_ejs_Int16Array") },
        "Int32Array":         { get: () => getGlobal("_ejs_Int32Array") },
        "Uint8Array":         { get: () => getGlobal("_ejs_Uint8Array") },
        "Uint16Array":        { get: () => getGlobal("_ejs_Uint16Array") },
        "Uint32Array":        { get: () => getGlobal("_ejs_Uint32Array") },
        "Float32Array":       { get: () => getGlobal("_ejs_Float32Array") },
        "Float64Array":       { get: () => getGlobal("_ejs_Float64Array") },
        "XMLHttpRequest":     { get: () => getGlobal("_ejs_XMLHttpRequest") },
        "process":            { get: () => getGlobal("_ejs_Process") },
        "require":            { get: () => getGlobal("_ejs_require") },
        "isNaN":              { get: () => getGlobal("_ejs_isNaN") },
        "isFinite":           { get: () => getGlobal("_ejs_isFinite") },
        "parseInt":           { get: () => getGlobal("_ejs_parseInt") },
        "parseFloat":         { get: () => getGlobal("_ejs_parseFloat") },
        "decodeURI":          { get: () => getGlobal("_ejs_decodeURI") },
        "encodeURI":          { get: () => getGlobal("_ejs_encodeURI") },
        "decodeURIComponent": { get: () => getGlobal("_ejs_decodeURIComponent") },
        "encodeURIComponent": { get: () => getGlobal("_ejs_encodeURIComponent") },
        "undefined":          { get: () => getGlobal("_ejs_undefined") },
        "Infinity":           { get: () => getGlobal("_ejs_Infinity") },
        "NaN":                { get: () => getGlobal("_ejs_nan") },
        "__ejs":              { get: () => getGlobal("_ejs__ejs") },
        // kind of a hack, but since we don't define these...
        "window":             { get: () => getGlobal("_ejs_undefined") },
        "document":           { get: () => getGlobal("_ejs_undefined") }
    });
}

export function createSymbolsInterface (module) {
    let getGlobal = (n) => module.getOrInsertGlobal(n, types.EjsValue);
    return Object.create (null, {
        "create":      { get: () => getGlobal("_ejs_Symbol_create") }
    });
}
