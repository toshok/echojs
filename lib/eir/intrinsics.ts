/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// The %-intrinsic calls EIR knows how to lower, keyed by callee name.
// Pre-EIR desugar passes (see preEIRConvert in desugar.js) rewrite
// constructs lowering has no native form for into calls of these
// intrinsics.
//
// scopes.ts consults this table to reject unknown intrinsics EARLY (a
// late LowerNotSupported is a compile error with less context), so keep
// it the single source of truth: never lower an intrinsic in lower.ts
// that isn't listed here.

import type { OpName } from "./ops";

interface OpIntrinsic {
    // lower to this op; operands = the visited arguments
    op: OpName;
    runtime?: undefined;
    // the op's result becomes the function's `this` (super() in a
    // derived constructor initializes it)
    rebindThis?: boolean;
    void?: undefined;
}

interface RuntimeIntrinsic {
    op?: undefined;
    // lower to call_runtime imms.name; the runtime function must take
    // plain ejsval arguments and return an ejsval
    runtime: string;
    rebindThis?: undefined;
    // the runtime function returns void (the call is only valid as a
    // statement; its EIR value reads as undefined)
    void?: boolean;
}

export type IntrinsicEntry = OpIntrinsic | RuntimeIntrinsic;

export const eir_intrinsics: Record<string, IntrinsicEntry> = {
    "%arrayFromSpread": { op: "array_from_spread" },

    // DesugarClasses
    "%objectCreate": { runtime: "object_create" },
    "%setPrototypeOf": { runtime: "object_set_prototype_of" },
    "%setConstructorKindBase": { runtime: "set_constructor_kind_base", void: true },
    "%setConstructorKindDerived": { runtime: "set_constructor_kind_derived", void: true },
    "%constructSuper": { op: "construct_super", rebindThis: true },
    "%constructSuperApply": { op: "construct_super_apply", rebindThis: true },

    // DesugarSpread (new Foo(...args))
    "%constructApply": { op: "construct_apply" },

    // DesugarMetaProperties (new.target)
    "%getNewTarget": { op: "new_target" },

    // DesugarGeneratorFunctions: the state-machine generators
    // (docs/generator-eir-plan.md).  %generatorYield and
    // %generatorDelegate lower specially in lower.ts (a gen_yield op /
    // an inline delegation loop — they only exist inside marked bodies);
    // their entries here are for scopes.ts name acceptance and never
    // emit as runtime calls.
    "%makeGeneratorEIR": { runtime: "make_generator_eir" },
    "%generatorYield": { runtime: "make_generator_eir" },
    "%generatorDelegate": { runtime: "make_generator_eir" },
    "%generatorIsReturnSentinel": { runtime: "generator_is_return_sentinel" },
    "%generatorReturnValue": { runtime: "generator_return_value" },

    // DesugarDestructuring (array patterns iterate via a runtime wrapper)
    "%createIteratorWrapper": { runtime: "iterator_wrapper_new" },

    // DesugarAsyncFunctions: async-generator definitions join the
    // %AsyncGeneratorFunction% prototype chain
    "%markAsyncGen": { runtime: "mark_async_generator" },

    // DesugarSpread / DesugarDestructuring: object spread + object rest
    "%copyDataProps": { runtime: "copy_data_properties" },
    // DesugarDestructuring: object patterns TypeError on null/undefined
    // RHS even when the pattern reads no properties ({} = undefined)
    "%requireObjectCoercible": { runtime: "require_object_coercible" },
    "%bigintFromLiteral": { runtime: "bigint_from_literal" },
    "%objectSpreadMerge": { runtime: "object_spread_merge" },

    // DesugarClasses: class fields + private members
    "%defineField": { runtime: "define_field" },
    "%makePrivateMap": { runtime: "make_private_map" },
    "%privFieldGet": { runtime: "private_field_get" },
    "%privFieldSet": { runtime: "private_field_set" },
    "%privFieldInit": { runtime: "private_field_init" },
    "%privBrandCheck": { runtime: "private_brand_check" },
    "%privHas": { runtime: "private_has" },
    "%privWriteError": { runtime: "private_write_error" },
};
