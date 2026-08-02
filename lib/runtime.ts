/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// The compiler's window into the C runtime: every entry declares one
// extern function (or global) with its LLVM signature.  Interfaces are
// built as getter objects so a function is only declared in a module
// when something actually references it.

import * as ty from "./types";
import * as llvm from "@llvm";
import type { ABI } from "./abi";

const takes_builtins = ty.takes_builtins;
const does_not_throw = ty.does_not_throw;
const does_not_access_memory = ty.does_not_access_memory;
const only_reads_memory = ty.only_reads_memory;
const returns_ejsval_bool = ty.returns_ejsval_bool;

// getters run with the interface object (module + abi) as `this`
export interface RuntimeContext {
    module: llvm.Module;
    abi: ABI;
}

const runtime_interface = {
    personality: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "__ejs_personality_v0", ty.Int32, [
            ty.Int32,
            ty.Int32,
            ty.Int64,
            ty.Int8Pointer,
            ty.Int8Pointer,
        ]);
    },

    module_resolve: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_module_resolve", ty.Void, [
            ty.EjsModule.pointerTo(),
        ]);
    },
    module_get: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_module_get", ty.EjsValue, [
            ty.EjsValue,
        ]);
    },
    module_get_slot_ref: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_module_get_slot_ref",
            ty.EjsValue.pointerTo(),
            [ty.EjsModule.pointerTo(), ty.Int32]
        );
    },
    module_add_export_accessors: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_module_add_export_accessors",
            ty.Void,
            [
                ty.EjsModule.pointerTo(),
                ty.String,
                ty.getEjsClosureFunc(this.abi),
                ty.getEjsClosureFunc(this.abi),
            ]
        );
    },

    invoke_closure: function (this: RuntimeContext) {
        return takes_builtins(
            this.abi.createExternalFunction(this.module, "_ejs_invoke_closure", ty.EjsValue, [
                ty.EjsValue,
                ty.EjsValue.pointerTo(),
                ty.Int32,
                ty.EjsValue.pointerTo(),
                ty.EjsValue,
            ])
        );
    },
    construct_closure: function (this: RuntimeContext) {
        return takes_builtins(
            this.abi.createExternalFunction(this.module, "_ejs_construct_closure", ty.EjsValue, [
                ty.EjsValue,
                ty.EjsValue.pointerTo(),
                ty.Int32,
                ty.EjsValue.pointerTo(),
                ty.EjsValue,
            ])
        );
    },
    construct_closure_apply: function (this: RuntimeContext) {
        return takes_builtins(
            this.abi.createExternalFunction(
                this.module,
                "_ejs_construct_closure_apply",
                ty.EjsValue,
                [
                    ty.EjsValue,
                    ty.EjsValue.pointerTo(),
                    ty.Int32,
                    ty.EjsValue.pointerTo(),
                    ty.EjsValue,
                ]
            )
        );
    },

    set_constructor_kind_derived: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_function_set_derived_constructor",
            ty.Void,
            [ty.EjsValue]
        );
    },
    set_constructor_kind_base: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_function_set_base_constructor",
            ty.Void,
            [ty.EjsValue]
        );
    },

    bigint_from_literal: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_bigint_from_literal", ty.EjsValue, [
            ty.EjsValue,
        ]);
    },

    op_to_numeric: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_op_to_numeric", ty.EjsValue, [
            ty.EjsValue,
        ]);
    },
    op_add_update: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_op_add_update", ty.EjsValue, [
            ty.EjsValue,
            ty.EjsValue,
        ]);
    },
    op_sub_update: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_op_sub_update", ty.EjsValue, [
            ty.EjsValue,
            ty.EjsValue,
        ]);
    },

    make_closure: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_function_new_closure",
            ty.EjsValue,
            [ty.EjsValue, ty.EjsValue, ty.getEjsClosureFunc(this.abi), ty.Int32]
        );
    },
    make_closure_noenv: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_function_new_without_env",
            ty.EjsValue,
            [ty.EjsValue, ty.getEjsClosureFunc(this.abi)]
        );
    },
    make_anon_closure: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_function_new_anon", ty.EjsValue, [
            ty.EjsValue,
            ty.getEjsClosureFunc(this.abi),
        ]);
    },

    make_closure_env: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_closureenv_new", ty.EjsValue, [
            ty.Int32,
        ]);
    },
    get_env_slot_val: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_closureenv_get_slot",
            ty.EjsValue,
            [ty.EjsValue, ty.Int32]
        );
    },
    get_env_slot_ref: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_closureenv_get_slot_ref",
            ty.EjsValue.pointerTo(),
            [ty.EjsValue, ty.Int32]
        );
    },

    // the out-of-line half of the emitted write barrier
    // (object-remembering: the OWNER ejsval, not the slot)
    gc_write_barrier: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_gc_remember_val",
            ty.Void,
            [ty.EjsValue, ty.EjsValue]
        );
    },

    make_generator: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_generator_new", ty.EjsValue, [
            ty.EjsValue,
        ]);
    },
    generator_is_return_sentinel: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_generator_is_return_sentinel",
            ty.EjsValue,
            [ty.EjsValue]
        );
    },
    generator_return_value: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_generator_return_value",
            ty.EjsValue,
            [ty.EjsValue]
        );
    },
    generator_yield: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_generator_yield", ty.EjsValue, [
            ty.EjsValue,
            ty.EjsValue,
        ]);
    },

    define_field: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_define_field", ty.EjsValue, [
            ty.EjsValue,
            ty.EjsValue,
            ty.EjsValue,
        ]);
    },
    make_private_map: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_weakmap_new", ty.EjsValue, []);
    },
    private_field_get: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_private_field_get", ty.EjsValue, [
            ty.EjsValue,
            ty.EjsValue,
            ty.EjsValue,
        ]);
    },
    private_field_set: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_private_field_set", ty.EjsValue, [
            ty.EjsValue,
            ty.EjsValue,
            ty.EjsValue,
            ty.EjsValue,
        ]);
    },
    private_field_init: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_private_field_init",
            ty.EjsValue,
            [ty.EjsValue, ty.EjsValue, ty.EjsValue]
        );
    },
    private_brand_check: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_private_brand_check",
            ty.EjsValue,
            [ty.EjsValue, ty.EjsValue, ty.EjsValue]
        );
    },
    private_has: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_private_has", ty.EjsValue, [
            ty.EjsValue,
            ty.EjsValue,
        ]);
    },
    private_write_error: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_private_write_error",
            ty.EjsValue,
            [ty.EjsValue]
        );
    },
    copy_data_properties: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_copy_data_properties",
            ty.EjsValue,
            [ty.EjsValue, ty.EjsValue, ty.EjsValue]
        );
    },
    require_object_coercible: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_require_object_coercible",
            ty.EjsValue,
            [ty.EjsValue]
        );
    },
    object_spread_merge: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_object_spread_merge",
            ty.EjsValue,
            [ty.EjsValue, ty.EjsValue]
        );
    },
    object_create: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_object_create", ty.EjsValue, [
            ty.EjsValue,
        ]);
    },
    arguments_new: function (this: RuntimeContext) {
        return does_not_throw(
            this.abi.createExternalFunction(this.module, "_ejs_arguments_new", ty.EjsValue, [
                ty.Int32,
                ty.EjsValue.pointerTo(),
            ])
        );
    },
    array_new: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_array_new", ty.EjsValue, [
            ty.Int64,
            ty.Bool,
        ]);
    },
    array_new_copy: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_array_new_copy", ty.EjsValue, [
            ty.Int64,
            ty.EjsValue.pointerTo(),
        ]);
    },
    array_from_iterables: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_array_from_iterables",
            ty.EjsValue,
            [ty.Int32, ty.EjsValue.pointerTo()]
        );
    },
    arg_length: function (this: RuntimeContext) {
        return does_not_throw(
            does_not_access_memory(
                this.abi.createExternalFunction(this.module, "_ejs_arg_length", ty.EjsValue, [
                    ty.Int32,
                    ty.Int32,
                ])
            )
        );
    },
    number_new: function (this: RuntimeContext) {
        return does_not_throw(
            does_not_access_memory(
                this.abi.createExternalFunction(this.module, "_ejs_number_new", ty.EjsValue, [
                    ty.Double,
                ])
            )
        );
    },
    string_new_utf8: function (this: RuntimeContext) {
        return only_reads_memory(
            does_not_throw(
                this.abi.createExternalFunction(this.module, "_ejs_string_new_utf8", ty.EjsValue, [
                    ty.String,
                ])
            )
        );
    },
    regexp_new_utf8: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_regexp_new_utf8", ty.EjsValue, [
            ty.String,
            ty.String,
        ]);
    },
    truthy: function (this: RuntimeContext) {
        return does_not_throw(
            does_not_access_memory(
                this.abi.createExternalFunction(this.module, "_ejs_truthy", ty.Bool, [ty.EjsValue])
            )
        );
    },
    object_setprop: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_object_setprop", ty.EjsValue, [
            ty.EjsValue,
            ty.EjsValue,
            ty.EjsValue,
        ]);
    },
    object_getprop: function (this: RuntimeContext) {
        return only_reads_memory(
            this.abi.createExternalFunction(this.module, "_ejs_object_getprop", ty.EjsValue, [
                ty.EjsValue,
                ty.EjsValue,
            ])
        );
    },
    // born-with-shape: batched literal allocation and
    // fenced-constructor prefix fill.  argc, names*, values*.
    object_new_shaped: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_object_new_shaped", ty.EjsValue, [
            ty.Int32,
            ty.EjsValue.pointerTo(),
            ty.EjsValue.pointerTo(),
        ]);
    },
    object_fill_shaped: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_object_fill_shaped",
            ty.EjsValue,
            [ty.EjsValue, ty.Int32, ty.EjsValue.pointerTo(), ty.EjsValue.pointerTo()]
        );
    },
    global_setprop: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_global_setprop", ty.EjsValue, [
            ty.EjsValue,
            ty.EjsValue,
        ]);
    },
    sloppy_this: function (this: RuntimeContext) {
        return only_reads_memory(
            this.abi.createExternalFunction(this.module, "_ejs_sloppy_this", ty.EjsValue, [
                ty.EjsValue,
            ])
        );
    },
    mark_async_generator: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_mark_async_generator", ty.EjsValue, [
            ty.EjsValue,
        ]);
    },
    // strict-mode member store: throws TypeError on a failed [[Set]]
    object_setprop_strict: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_object_setprop_strict", ty.EjsValue, [
            ty.EjsValue,
            ty.EjsValue,
            ty.EjsValue,
        ]);
    },
    // strict-mode assignment to an unresolvable name throws ReferenceError
    global_setprop_strict: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_global_setprop_strict", ty.EjsValue, [
            ty.EjsValue,
            ty.EjsValue,
        ]);
    },
    global_getprop: function (this: RuntimeContext) {
        return only_reads_memory(
            this.abi.createExternalFunction(this.module, "_ejs_global_getprop", ty.EjsValue, [
                ty.EjsValue,
            ])
        );
    },
    // plain reads: throws ReferenceError on an unresolvable name, so no
    // only_reads_memory attribute
    global_getprop_checked: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_global_getprop_checked", ty.EjsValue, [
            ty.EjsValue,
        ]);
    },

    object_define_accessor_prop: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_object_define_accessor_property",
            ty.Bool,
            [ty.EjsValue, ty.EjsValue, ty.EjsValue, ty.EjsValue, ty.Int32]
        );
    },
    object_define_accessor_prop_desc: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_object_define_accessor_property_desc",
            ty.Bool,
            [ty.EjsValue, ty.EjsValue, ty.EjsValue, ty.EjsValue, ty.Int32]
        );
    },
    object_define_value_prop: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_object_define_value_property",
            ty.Bool,
            [ty.EjsValue, ty.EjsValue, ty.EjsValue, ty.Int32]
        );
    },

    object_freeze: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_object_freeze", ty.EjsValue, [
            ty.EjsValue,
        ]);
    },
    object_literal_set_proto: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_object_literal_set_proto",
            ty.EjsValue,
            [ty.EjsValue, ty.EjsValue]
        );
    },
    object_set_prototype_of: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_object_set_prototype_of",
            ty.EjsValue,
            [ty.EjsValue, ty.EjsValue]
        );
    },
    prop_iterator_new: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_property_iterator_new",
            ty.EjsPropIterator,
            [ty.EjsValue]
        );
    },
    prop_iterator_current: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_property_iterator_current",
            ty.EjsValue,
            [ty.EjsPropIterator]
        );
    },
    prop_iterator_next: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_property_iterator_next",
            ty.Bool,
            [ty.EjsPropIterator, ty.Bool]
        );
    },
    begin_catch: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_begin_catch", ty.EjsValue, [
            ty.Int8Pointer,
        ]);
    },
    end_catch: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_end_catch", ty.EjsValue, []);
    },
    throw_nativeerror_utf8: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_throw_nativeerror_utf8",
            ty.Void,
            [ty.Int32, ty.String]
        );
    },
    throw: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_throw", ty.Void, [ty.EjsValue]);
    },
    rethrow: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_rethrow", ty.Void, [ty.EjsValue]);
    },

    ToString: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "ToString", ty.EjsValue, [ty.EjsValue]);
    },
    string_concat: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_string_concat", ty.EjsValue, [
            ty.EjsValue,
            ty.EjsValue,
        ]);
    },
    // module-init interning of guard shapes (names are
    // this module's atoms; f64_mask bit i = field i has repr f64).
    // Returns the interned shape index, or EJS_SHAPE_NOMATCH.
    shape_intern: function (this: RuntimeContext) {
        return does_not_throw(
            this.abi.createExternalFunction(this.module, "_ejs_shape_intern", ty.Int32, [
                ty.Int32,
                ty.EjsValue.pointerTo(),
                ty.Int32,
            ])
        );
    },
    init_string_literal: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_string_init_literal", ty.Void, [
            ty.String,
            ty.EjsValue.pointerTo(),
            ty.EjsPrimString.pointerTo(),
            ty.JSChar.pointerTo(),
            ty.Int32,
        ]);
    },

    gc_add_root: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_gc_add_root", ty.Void, [
            ty.EjsValue.pointerTo(),
        ]);
    },
    typeof_is_object: function (this: RuntimeContext) {
        return returns_ejsval_bool(
            only_reads_memory(
                this.abi.createExternalFunction(
                    this.module,
                    "_ejs_op_typeof_is_object",
                    ty.EjsValue,
                    [ty.EjsValue]
                )
            )
        );
    },
    typeof_is_function: function (this: RuntimeContext) {
        return returns_ejsval_bool(
            only_reads_memory(
                this.abi.createExternalFunction(
                    this.module,
                    "_ejs_op_typeof_is_function",
                    ty.EjsValue,
                    [ty.EjsValue]
                )
            )
        );
    },
    typeof_is_string: function (this: RuntimeContext) {
        return returns_ejsval_bool(
            only_reads_memory(
                this.abi.createExternalFunction(
                    this.module,
                    "_ejs_op_typeof_is_string",
                    ty.EjsValue,
                    [ty.EjsValue]
                )
            )
        );
    },
    typeof_is_number: function (this: RuntimeContext) {
        return returns_ejsval_bool(
            only_reads_memory(
                this.abi.createExternalFunction(
                    this.module,
                    "_ejs_op_typeof_is_number",
                    ty.EjsValue,
                    [ty.EjsValue]
                )
            )
        );
    },
    typeof_is_undefined: function (this: RuntimeContext) {
        return returns_ejsval_bool(
            only_reads_memory(
                this.abi.createExternalFunction(
                    this.module,
                    "_ejs_op_typeof_is_undefined",
                    ty.EjsValue,
                    [ty.EjsValue]
                )
            )
        );
    },
    typeof_is_null: function (this: RuntimeContext) {
        return returns_ejsval_bool(
            only_reads_memory(
                this.abi.createExternalFunction(
                    this.module,
                    "_ejs_op_typeof_is_null",
                    ty.EjsValue,
                    [ty.EjsValue]
                )
            )
        );
    },
    typeof_is_boolean: function (this: RuntimeContext) {
        return returns_ejsval_bool(
            only_reads_memory(
                this.abi.createExternalFunction(
                    this.module,
                    "_ejs_op_typeof_is_boolean",
                    ty.EjsValue,
                    [ty.EjsValue]
                )
            )
        );
    },

    create_iter_result: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_create_iter_result",
            ty.EjsValue,
            [ty.EjsValue, ty.EjsValue]
        );
    },

    iterator_wrapper_new: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(
            this.module,
            "_ejs_iterator_wrapper_new",
            ty.EjsValue,
            [ty.EjsValue]
        );
    },

    undefined: function (this: RuntimeContext) {
        return this.module.getOrInsertGlobal("_ejs_undefined", ty.EjsValue);
    },
    true: function (this: RuntimeContext) {
        return this.module.getOrInsertGlobal("_ejs_true", ty.EjsValue);
    },
    false: function (this: RuntimeContext) {
        return this.module.getOrInsertGlobal("_ejs_false", ty.EjsValue);
    },
    null: function (this: RuntimeContext) {
        return this.module.getOrInsertGlobal("_ejs_null", ty.EjsValue);
    },
    one: function (this: RuntimeContext) {
        return this.module.getOrInsertGlobal("_ejs_one", ty.EjsValue);
    },
    zero: function (this: RuntimeContext) {
        return this.module.getOrInsertGlobal("_ejs_zero", ty.EjsValue);
    },
    global: function (this: RuntimeContext) {
        return this.module.getOrInsertGlobal("_ejs_global", ty.EjsValue);
    },
    exception_typeinfo: function (this: RuntimeContext) {
        return this.module.getOrInsertGlobal("EJS_EHTYPE_ejsvalue", ty.EjsExceptionTypeInfo);
    },
    function_specops: function (this: RuntimeContext) {
        return this.module.getOrInsertGlobal("_ejs_Function_specops", ty.EjsSpecops);
    },
    symbol_specops: function (this: RuntimeContext) {
        return this.module.getOrInsertGlobal("_ejs_Symbol_specops", ty.EjsSpecops);
    },

    "unop-": function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_op_neg", ty.EjsValue, [
            ty.EjsValue,
        ]);
    },
    "unop+": function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_op_plus", ty.EjsValue, [
            ty.EjsValue,
        ]);
    },
    "unop!": function (this: RuntimeContext) {
        return returns_ejsval_bool(
            this.abi.createExternalFunction(this.module, "_ejs_op_not", ty.EjsValue, [ty.EjsValue])
        );
    },
    "unop~": function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_op_bitwise_not", ty.EjsValue, [
            ty.EjsValue,
        ]);
    },
    unoptypeof: function (this: RuntimeContext) {
        return does_not_throw(
            this.abi.createExternalFunction(this.module, "_ejs_op_typeof", ty.EjsValue, [
                ty.EjsValue,
            ])
        );
    },
    unopdelete: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_op_delete", ty.EjsValue, [
            ty.EjsValue,
            ty.EjsValue,
        ]);
    }, // this is a unop, but ours only works for memberexpressions
    unopdelete_strict: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_op_delete_strict", ty.EjsValue, [
            ty.EjsValue,
            ty.EjsValue,
        ]);
    },
    unopvoid: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_op_void", ty.EjsValue, [
            ty.EjsValue,
        ]);
    },

    dump_value: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_dump_value", ty.Void, [
            ty.EjsValue,
        ]);
    },
    log: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_logstr", ty.Void, [ty.String]);
    },
    record_binop: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_record_binop", ty.Void, [
            ty.Int32,
            ty.String,
            ty.EjsValue,
            ty.EjsValue,
        ]);
    },
    record_assignment: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_record_assignment", ty.Void, [
            ty.Int32,
            ty.EjsValue,
        ]);
    },
    record_getprop: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_record_getprop", ty.Void, [
            ty.Int32,
            ty.EjsValue,
            ty.EjsValue,
        ]);
    },
    record_setprop: function (this: RuntimeContext) {
        return this.abi.createExternalFunction(this.module, "_ejs_record_setprop", ty.Void, [
            ty.Int32,
            ty.EjsValue,
            ty.EjsValue,
            ty.EjsValue,
        ]);
    },
} satisfies Record<string, (this: RuntimeContext) => llvm.EjsFunction | llvm.GlobalVariable>;

export type RuntimeInterface = RuntimeContext & {
    readonly [K in keyof typeof runtime_interface]: ReturnType<(typeof runtime_interface)[K]>;
};

export function createInterface(module: llvm.Module, abi: ABI): RuntimeInterface {
    const runtime = { module, abi } as RuntimeInterface;

    for (const k of Object.keys(runtime_interface) as (keyof typeof runtime_interface)[])
        Object.defineProperty(runtime, k, { get: runtime_interface[k] });
    return runtime;
}

export function createBinopsInterface(
    module: llvm.Module,
    abi: ABI
): Record<string, llvm.EjsFunction> {
    const createBinop = (n: string) =>
        abi.createExternalFunction(module, n, ty.EjsValue, [ty.EjsValue, ty.EjsValue]);
    return Object.create(null, {
        "^": { get: () => createBinop("_ejs_op_bitwise_xor") },
        "&": { get: () => createBinop("_ejs_op_bitwise_and") },
        "|": { get: () => createBinop("_ejs_op_bitwise_or") },
        ">>": { get: () => createBinop("_ejs_op_rsh") },
        "<<": { get: () => createBinop("_ejs_op_lsh") },
        ">>>": { get: () => createBinop("_ejs_op_ursh") },
        "<<<": { get: () => createBinop("_ejs_op_ulsh") },
        "%": { get: () => createBinop("_ejs_op_mod") },
        "**": { get: () => createBinop("_ejs_op_exp") },
        "+": { get: () => createBinop("_ejs_op_add") },
        "*": { get: () => createBinop("_ejs_op_mult") },
        "/": { get: () => createBinop("_ejs_op_div") },
        "-": { get: () => createBinop("_ejs_op_sub") },
        "<": { get: () => returns_ejsval_bool(createBinop("_ejs_op_lt")) },
        "<=": { get: () => returns_ejsval_bool(createBinop("_ejs_op_le")) },
        ">": { get: () => returns_ejsval_bool(createBinop("_ejs_op_gt")) },
        ">=": { get: () => returns_ejsval_bool(createBinop("_ejs_op_ge")) },
        "===": {
            get: () => returns_ejsval_bool(createBinop("_ejs_op_strict_eq")),
        },
        "==": { get: () => returns_ejsval_bool(createBinop("_ejs_op_eq")) },
        "!==": {
            get: () => returns_ejsval_bool(createBinop("_ejs_op_strict_neq")),
        },
        "!=": { get: () => returns_ejsval_bool(createBinop("_ejs_op_neq")) },
        instanceof: {
            get: () => returns_ejsval_bool(createBinop("_ejs_op_instanceof")),
        },
        in: { get: () => returns_ejsval_bool(createBinop("_ejs_op_in")) },
    });
}

export function createAtomsInterface(module: llvm.Module): Record<string, llvm.GlobalVariable> {
    const getGlobal = (n: string) => module.getOrInsertGlobal(n, ty.EjsValue);
    return Object.create(null, {
        null: { get: () => getGlobal("_ejs_atom_null") },
        undefined: { get: () => getGlobal("_ejs_atom_undefined") },
        constructor: { get: () => getGlobal("_ejs_atom_constructor") },
        length: { get: () => getGlobal("_ejs_atom_length") },
        __ejs: { get: () => getGlobal("_ejs_atom___ejs") },
        object: { get: () => getGlobal("_ejs_atom_object") },
        function: { get: () => getGlobal("_ejs_atom_function") },
        prototype: { get: () => getGlobal("_ejs_atom_prototype") },
        console: { get: () => getGlobal("_ejs_atom_console") },
        log: { get: () => getGlobal("_ejs_atom_log") },
        time: { get: () => getGlobal("_ejs_atom_time") },
        timeEnd: { get: () => getGlobal("_ejs_atom_timeEnd") },
        message: { get: () => getGlobal("_ejs_atom_message") },
        name: { get: () => getGlobal("_ejs_atom_name") },
        true: { get: () => getGlobal("_ejs_atom_true") },
        false: { get: () => getGlobal("_ejs_atom_false") },
        Arguments: { get: () => getGlobal("_ejs_atom_Arguments") },
        Array: { get: () => getGlobal("_ejs_atom_Array") },
        Boolean: { get: () => getGlobal("_ejs_atom_Boolean") },
        Date: { get: () => getGlobal("_ejs_atom_Date") },
        Function: { get: () => getGlobal("_ejs_atom_Function") },
        JSON: { get: () => getGlobal("_ejs_atom_JSON") },
        Math: { get: () => getGlobal("_ejs_atom_Math") },
        Number: { get: () => getGlobal("_ejs_atom_Number") },
        Object: { get: () => getGlobal("_ejs_atom_Object") },
        Reflect: { get: () => getGlobal("_ejs_atom_Reflect") },
        RegExp: { get: () => getGlobal("_ejs_atom_RegExp") },
        String: { get: () => getGlobal("_ejs_atom_String") },
        Promise: { get: () => getGlobal("_ejs_atom_Promise") },
        Error: { get: () => getGlobal("_ejs_atom_Error") },
        EvalError: { get: () => getGlobal("_ejs_atom_EvalError") },
        RangeError: { get: () => getGlobal("_ejs_atom_RangeError") },
        ReferenceError: { get: () => getGlobal("_ejs_atom_ReferenceError") },
        SyntaxError: { get: () => getGlobal("_ejs_atom_SyntaxError") },
        TypeError: { get: () => getGlobal("_ejs_atom_TypeError") },
        URIError: { get: () => getGlobal("_ejs_atom_URIError") },
        exports: { get: () => getGlobal("_ejs_atom_exports") },
        assign: { get: () => getGlobal("_ejs_atom_assign") },
        getPrototypeOf: { get: () => getGlobal("_ejs_atom_getPrototypeOf") },
        setPrototypeOf: { get: () => getGlobal("_ejs_atom_setPrototypeOf") },
        getOwnPropertyDescriptor: {
            get: () => getGlobal("_ejs_atom_getOwnPropertyDescriptor"),
        },
        getOwnPropertyNames: {
            get: () => getGlobal("_ejs_atom_getOwnPropertyNames"),
        },
        getOwnPropertySymbols: {
            get: () => getGlobal("_ejs_atom_getOwnPropertySymbols"),
        },
        create: { get: () => getGlobal("_ejs_atom_create") },
        defineProperty: { get: () => getGlobal("_ejs_atom_defineProperty") },
        defineProperties: {
            get: () => getGlobal("_ejs_atom_defineProperties"),
        },
        seal: { get: () => getGlobal("_ejs_atom_seal") },
        freeze: { get: () => getGlobal("_ejs_atom_freeze") },
        preventExtensions: {
            get: () => getGlobal("_ejs_atom_preventExtensions"),
        },
        isSealed: { get: () => getGlobal("_ejs_atom_isSealed") },
        isFrozen: { get: () => getGlobal("_ejs_atom_isFrozen") },
        isExtensible: { get: () => getGlobal("_ejs_atom_isExtensible") },
        keys: { get: () => getGlobal("_ejs_atom_keys") },
        toString: { get: () => getGlobal("_ejs_atom_toString") },
        toLocaleString: { get: () => getGlobal("_ejs_atom_toLocaleString") },
        valueOf: { get: () => getGlobal("_ejs_atom_valueOf") },
        hasOwnProperty: { get: () => getGlobal("_ejs_atom_hasOwnProperty") },
        isPrototypeOf: { get: () => getGlobal("_ejs_atom_isPrototypeOf") },
        propertyIsEnumerable: {
            get: () => getGlobal("_ejs_atom_propertyIsEnumerable"),
        },
        isArray: { get: () => getGlobal("_ejs_atom_isArray") },
        push: { get: () => getGlobal("_ejs_atom_push") },
        pop: { get: () => getGlobal("_ejs_atom_pop") },
        shift: { get: () => getGlobal("_ejs_atom_shift") },
        unshift: { get: () => getGlobal("_ejs_atom_unshift") },
        concat: { get: () => getGlobal("_ejs_atom_concat") },
        slice: { get: () => getGlobal("_ejs_atom_slice") },
        splice: { get: () => getGlobal("_ejs_atom_splice") },
        indexOf: { get: () => getGlobal("_ejs_atom_indexOf") },
        join: { get: () => getGlobal("_ejs_atom_join") },
        forEach: { get: () => getGlobal("_ejs_atom_forEach") },
        map: { get: () => getGlobal("_ejs_atom_map") },
        every: { get: () => getGlobal("_ejs_atom_every") },
        some: { get: () => getGlobal("_ejs_atom_some") },
        reduce: { get: () => getGlobal("_ejs_atom_reduce") },
        reduceRight: { get: () => getGlobal("_ejs_atom_reduceRight") },
        reverse: { get: () => getGlobal("_ejs_atom_reverse") },
        copyWithin: { get: () => getGlobal("_ejs_atom_copyWithin") },
        of: { get: () => getGlobal("_ejs_atom_of") },
        from: { get: () => getGlobal("_ejs_atom_from") },
        fill: { get: () => getGlobal("_ejs_atom_fill") },
        find: { get: () => getGlobal("_ejs_atom_find") },
        findIndex: { get: () => getGlobal("_ejs_atom_findIndex") },
        filter: { get: () => getGlobal("_ejs_atom_filter") },
        next: { get: () => getGlobal("_ejs_atom_next") },
        done: { get: () => getGlobal("_ejs_atom_done") },
        parse: { get: () => getGlobal("_ejs_atom_parse") },
        stringify: { get: () => getGlobal("_ejs_atom_stringify") },
        charAt: { get: () => getGlobal("_ejs_atom_charAt") },
        charCodeAt: { get: () => getGlobal("_ejs_atom_charCodeAt") },
        codePointAt: { get: () => getGlobal("_ejs_atom_codePointAt") },
        includes: { get: () => getGlobal("_ejs_atom_includes") },
        endsWith: { get: () => getGlobal("_ejs_atom_endsWith") },
        lastIndexOf: { get: () => getGlobal("_ejs_atom_lastIndexOf") },
        localeCompare: { get: () => getGlobal("_ejs_atom_localeCompare") },
        match: { get: () => getGlobal("_ejs_atom_match") },
        repeat: { get: () => getGlobal("_ejs_atom_repeat") },
        replace: { get: () => getGlobal("_ejs_atom_replace") },
        search: { get: () => getGlobal("_ejs_atom_search") },
        split: { get: () => getGlobal("_ejs_atom_split") },
        startsWith: { get: () => getGlobal("_ejs_atom_startsWith") },
        substr: { get: () => getGlobal("_ejs_atom_substr") },
        substring: { get: () => getGlobal("_ejs_atom_substring") },
        toLocaleLowerCase: {
            get: () => getGlobal("_ejs_atom_toLocaleLowerCase"),
        },
        toLocaleUpperCase: {
            get: () => getGlobal("_ejs_atom_toLocaleUpperCase"),
        },
        toLowerCase: { get: () => getGlobal("_ejs_atom_toLowerCase") },
        toUpperCase: { get: () => getGlobal("_ejs_atom_toUpperCase") },
        trim: { get: () => getGlobal("_ejs_atom_trim") },
        fromCharCode: { get: () => getGlobal("_ejs_atom_fromCharCode") },
        fromCodePoint: { get: () => getGlobal("_ejs_atom_fromCodePoint") },
        raw: { get: () => getGlobal("_ejs_atom_raw") },
        apply: { get: () => getGlobal("_ejs_atom_apply") },
        call: { get: () => getGlobal("_ejs_atom_call") },
        bind: { get: () => getGlobal("_ejs_atom_bind") },
        warn: { get: () => getGlobal("_ejs_atom_warn") },
        exit: { get: () => getGlobal("_ejs_atom_exit") },
        chdir: { get: () => getGlobal("_ejs_atom_chdir") },
        cwd: { get: () => getGlobal("_ejs_atom_cwd") },
        env: { get: () => getGlobal("_ejs_atom_env") },
        Map: { get: () => getGlobal("_ejs_atom_Map") },
        Set: { get: () => getGlobal("_ejs_atom_Set") },
        clear: { get: () => getGlobal("_ejs_atom_clear") },
        delete: { get: () => getGlobal("_ejs_atom_delete") },
        entries: { get: () => getGlobal("_ejs_atom_entries") },
        add: { get: () => getGlobal("_ejs_atom_add") },
        has: { get: () => getGlobal("_ejs_atom_has") },
        values: { get: () => getGlobal("_ejs_atom_values") },
        Proxy: { get: () => getGlobal("_ejs_atom_Proxy") },
        createFunction: { get: () => getGlobal("_ejs_atom_createFunction") },
        construct: { get: () => getGlobal("_ejs_atom_construct") },
        deleteProperty: { get: () => getGlobal("_ejs_atom_deleteProperty") },
        enumerate: { get: () => getGlobal("_ejs_atom_enumerate") },
        ownKeys: { get: () => getGlobal("_ejs_atom_ownKeys") },
        size: { get: () => getGlobal("_ejs_atom_size") },
        Symbol: { get: () => getGlobal("_ejs_atom_Symbol") },
        for: { get: () => getGlobal("_ejs_atom_for") },
        keyFor: { get: () => getGlobal("_ejs_atom_keyFor") },
        hasInstance: { get: () => getGlobal("_ejs_atom_hasInstance") },
        isConcatSpreadable: {
            get: () => getGlobal("_ejs_atom_isConcatSpreadable"),
        },
        species: { get: () => getGlobal("_ejs_atom_species") },
        iterator: { get: () => getGlobal("_ejs_atom_iterator") },
        toPrimitive: { get: () => getGlobal("_ejs_atom_toPrimitive") },
        toStringTag: { get: () => getGlobal("_ejs_atom_toStringTag") },
        unscopables: { get: () => getGlobal("_ejs_atom_unscopables") },
    });
}

export function createGlobalsInterface(module: llvm.Module): Record<string, llvm.GlobalVariable> {
    const getGlobal = (n: string) => module.getOrInsertGlobal(n, ty.EjsValue);
    return Object.create(null, {
        Object: { get: () => getGlobal("_ejs_Object") },
        Object_prototype: { get: () => getGlobal("_ejs_Object_prototype") },
        Boolean: { get: () => getGlobal("_ejs_Boolean") },
        String: { get: () => getGlobal("_ejs_String") },
        Number: { get: () => getGlobal("_ejs_Number") },
        Array: { get: () => getGlobal("_ejs_Array") },
        DataView: { get: () => getGlobal("_ejs_DataView") },
        Date: { get: () => getGlobal("_ejs_Date") },
        Error: { get: () => getGlobal("_ejs_Error") },
        EvalError: { get: () => getGlobal("_ejs_EvalError") },
        RangeError: { get: () => getGlobal("_ejs_RangeError") },
        ReferenceError: { get: () => getGlobal("_ejs_ReferenceError") },
        SyntaxError: { get: () => getGlobal("_ejs_SyntaxError") },
        TypeError: { get: () => getGlobal("_ejs_TypeError") },
        URIError: { get: () => getGlobal("_ejs_URIError") },
        Function: { get: () => getGlobal("_ejs_Function") },
        JSON: { get: () => getGlobal("_ejs_JSON") },
        Math: { get: () => getGlobal("_ejs_Math") },
        Map: { get: () => getGlobal("_ejs_Map") },
        WeakMap: { get: () => getGlobal("_ejs_WeakMap") },
        Proxy: { get: () => getGlobal("_ejs_Proxy") },
        Promise: { get: () => getGlobal("_ejs_Promise") },
        Reflect: { get: () => getGlobal("_ejs_Reflect") },
        RegExp: { get: () => getGlobal("_ejs_RegExp") },
        Symbol: { get: () => getGlobal("_ejs_Symbol") },
        Set: { get: () => getGlobal("_ejs_Set") },
        WeakSet: { get: () => getGlobal("_ejs_WeakSet") },
        console: { get: () => getGlobal("_ejs_console") },
        ArrayBuffer: { get: () => getGlobal("_ejs_ArrayBuffer") },
        Int8Array: { get: () => getGlobal("_ejs_Int8Array") },
        Int16Array: { get: () => getGlobal("_ejs_Int16Array") },
        Int32Array: { get: () => getGlobal("_ejs_Int32Array") },
        Uint8Array: { get: () => getGlobal("_ejs_Uint8Array") },
        Uint8ClampedArray: { get: () => getGlobal("_ejs_Uint8ClampedArray") },
        Uint16Array: { get: () => getGlobal("_ejs_Uint16Array") },
        Uint32Array: { get: () => getGlobal("_ejs_Uint32Array") },
        Float32Array: { get: () => getGlobal("_ejs_Float32Array") },
        Float64Array: { get: () => getGlobal("_ejs_Float64Array") },
        XMLHttpRequest: { get: () => getGlobal("_ejs_XMLHttpRequest") },
        process: { get: () => getGlobal("_ejs_Process") },
        require: { get: () => getGlobal("_ejs_require") },
        isNaN: { get: () => getGlobal("_ejs_isNaN") },
        isFinite: { get: () => getGlobal("_ejs_isFinite") },
        parseInt: { get: () => getGlobal("_ejs_parseInt") },
        parseFloat: { get: () => getGlobal("_ejs_parseFloat") },
        decodeURI: { get: () => getGlobal("_ejs_decodeURI") },
        encodeURI: { get: () => getGlobal("_ejs_encodeURI") },
        decodeURIComponent: { get: () => getGlobal("_ejs_decodeURIComponent") },
        encodeURIComponent: { get: () => getGlobal("_ejs_encodeURIComponent") },
        setInterval: { get: () => getGlobal("_ejs_setInterval") },
        setTimeout: { get: () => getGlobal("_ejs_setTimeout") },
        clearInterval: { get: () => getGlobal("_ejs_clearInterval") },
        clearTimeout: { get: () => getGlobal("_ejs_clearTimeout") },
        undefined: { get: () => getGlobal("_ejs_undefined") },
        Infinity: { get: () => getGlobal("_ejs_Infinity") },
        NaN: { get: () => getGlobal("_ejs_nan") },
        __ejs: { get: () => getGlobal("_ejs__ejs") },
        // kind of a hack, but since we don't define these...
        window: { get: () => getGlobal("_ejs_undefined") },
        document: { get: () => getGlobal("_ejs_undefined") },
    });
}

export function createSymbolsInterface(module: llvm.Module): Record<string, llvm.GlobalVariable> {
    const getGlobal = (n: string) => module.getOrInsertGlobal(n, ty.EjsValue);
    return Object.create(null, {
        create: { get: () => getGlobal("_ejs_Symbol_create") },
    });
}
