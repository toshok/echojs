/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

// The EIR opcode set and its effect table.  See EIRProposal.md.
//
// The effect table is the contract between lowering, the optimizer, and
// the abstract interpreter: every instruction's behavior with respect to
// the heap, exceptions, GC, and calls is declared here, not rediscovered
// by pattern matching.

export const Effect = {
    NONE: 0,
    READ: 1 << 0, // reads the JS heap
    WRITE: 1 << 1, // writes the JS heap
    THROW: 1 << 2, // may throw
    GC: 1 << 3, // may allocate / trigger a collection
    CALL: 1 << 4, // may reenter arbitrary JS
};

const E = Effect;

// effects shorthand for the generic operators: valueOf/toString hooks mean
// they can call back into JS, which implies read/write/throw/gc.
const GENERIC_OP = E.READ | E.WRITE | E.THROW | E.GC | E.CALL;

// arity: fixed operand count, or -1 for variadic.
// imms: names of immediate (non-value) attributes the instruction carries.
// terminator: ends a block; targets carry per-edge block-argument lists.
export const OPS = {
    // --- constants -------------------------------------------------------
    // imms.kind: "number" | "atom" | "boolean" | "undefined" | "null"
    // imms.value: the constant payload (unused for undefined/null)
    const: { arity: 0, effects: E.NONE, imms: ["kind", "value"] },

    // --- generic (high tier) operators ------------------------------------
    add: { arity: 2, effects: GENERIC_OP },
    sub: { arity: 2, effects: GENERIC_OP },
    mul: { arity: 2, effects: GENERIC_OP },
    div: { arity: 2, effects: GENERIC_OP },
    mod: { arity: 2, effects: GENERIC_OP },
    lt: { arity: 2, effects: GENERIC_OP },
    le: { arity: 2, effects: GENERIC_OP },
    gt: { arity: 2, effects: GENERIC_OP },
    ge: { arity: 2, effects: GENERIC_OP },
    loose_eq: { arity: 2, effects: GENERIC_OP },
    loose_neq: { arity: 2, effects: GENERIC_OP },
    bitand: { arity: 2, effects: GENERIC_OP },
    bitor: { arity: 2, effects: GENERIC_OP },
    bitxor: { arity: 2, effects: GENERIC_OP },
    shl: { arity: 2, effects: GENERIC_OP },
    shr: { arity: 2, effects: GENERIC_OP },
    ushr: { arity: 2, effects: GENERIC_OP },
    instanceof: { arity: 2, effects: GENERIC_OP },
    in: { arity: 2, effects: GENERIC_OP },
    neg: { arity: 1, effects: GENERIC_OP },
    unary_plus: { arity: 1, effects: GENERIC_OP },
    bitnot: { arity: 1, effects: GENERIC_OP },

    // pure predicates / conversions
    strict_eq: { arity: 2, effects: E.NONE },
    strict_neq: { arity: 2, effects: E.NONE },
    // to_boolean is pure in ejs (no valueOf involvement)
    to_boolean: { arity: 1, effects: E.NONE },
    typeof: { arity: 1, effects: E.GC },
    typeof_is: { arity: 1, effects: E.NONE, imms: ["type"] },
    logical_not: { arity: 1, effects: E.NONE },

    // --- properties --------------------------------------------------------
    get_prop: { arity: 2, effects: GENERIC_OP },
    set_prop: { arity: 3, effects: GENERIC_OP },
    get_prop_atom: { arity: 1, effects: GENERIC_OP, imms: ["atom"] },
    set_prop_atom: { arity: 2, effects: GENERIC_OP, imms: ["atom"] },
    delete_prop: { arity: 2, effects: GENERIC_OP },

    // --- globals ------------------------------------------------------------
    get_global: { arity: 0, effects: E.READ | E.THROW | E.GC, imms: ["atom"] },
    set_global: { arity: 1, effects: E.WRITE | E.THROW | E.GC, imms: ["atom"] },

    // --- closures / environments -------------------------------------------
    // make_env: operand 0 (optional, variadic 0..1) is the parent env
    make_env: { arity: -1, effects: E.GC, imms: ["size"] },
    env_load: { arity: 1, effects: E.READ, imms: ["slot"] },
    env_store: { arity: 2, effects: E.WRITE, imms: ["slot"] },
    // imms.fn = the EIR function to target; imms.name = the source-level
    // display name (Function.prototype.name) — the internal fn name is
    // scope-qualified and must not leak
    make_closure: { arity: 1, effects: E.GC, imms: ["fn", "name"] },

    // --- modules -------------------------------------------------------------
    module_slot_load: { arity: 0, effects: E.READ, imms: ["module", "slot"] },
    module_slot_store: { arity: 1, effects: E.WRITE, imms: ["module", "slot"] },
    module_get_exotic: { arity: 0, effects: E.READ | E.GC, imms: ["module"] },

    // --- calls ----------------------------------------------------------------
    // call: operands = [callee, this, ...args]
    // construct: operands = [callee, ...args]
    // either may carry targets [normal, unwind] when inside a protected
    // region, in which case it terminates its block.
    // call: [callee, this, ...args], or with imms.direct set (a direct
    // call to a known EIR function): [env, this, ...args]
    call: { arity: -1, effects: GENERIC_OP, may_terminate: true, imms: ["direct"] },
    construct: { arity: -1, effects: GENERIC_OP, may_terminate: true },
    // super(...) in a derived constructor: [super_ctor, ...args] (or
    // [super_ctor, args_array] for the _apply form).  calls the super
    // constructor with this function's incoming &this and newTarget,
    // writes the constructed object through &this, and returns it —
    // lowering rebinds `this` to the result.
    construct_super: { arity: -1, effects: GENERIC_OP, may_terminate: true },
    construct_super_apply: { arity: 2, effects: GENERIC_OP, may_terminate: true },
    // new Foo(...args): [ctor, args_array]; newTarget = the ctor itself
    construct_apply: { arity: 2, effects: GENERIC_OP, may_terminate: true },
    // the calling convention's newTarget argument (undefined unless
    // invoked via construct)
    new_target: { arity: 0, effects: E.NONE },

    // --- allocation ------------------------------------------------------------
    // dense: operands are the elements in order (no imms).  with holes:
    // imms.len = total length, imms.indices[i] = the array index operand i
    // lands at — holes stay holes (array_new force-fills, stores skip).
    make_array: { arity: -1, effects: E.GC | E.WRITE, imms: ["len", "indices"] },
    // %arrayFromSpread: concatenate the operands (each an array literal
    // chunk or an arbitrary iterable) into one fresh array.  iterating can
    // reenter user JS, hence GENERIC_OP.
    array_from_spread: { arity: -1, effects: GENERIC_OP },
    // imms.keys: array of atom names, one per operand
    make_object: { arity: -1, effects: E.GC | E.WRITE, imms: ["keys"] },
    // an accessor property on an object literal: [obj, getter, setter]
    // (undefined for a missing half); non-computed keys only
    define_accessor: { arity: 3, effects: E.GC | E.WRITE, imms: ["atom"] },
    // a fresh RegExp per evaluation (ES6 semantics, matching the legacy
    // visitLiteral); imms.source/imms.flags are strings
    make_regexp: { arity: 0, effects: E.THROW | E.GC, imms: ["source", "flags"] },
    // a tagged template's callsite object: frozen cooked-strings array
    // with a frozen .raw, built lazily into a per-site global (emit mints
    // the global; the same site always yields the identical object)
    template_callsite: { arity: 0, effects: E.GC | E.READ | E.WRITE, imms: ["cooked", "raw"] },
    // the rest-parameter array: arguments from index imms.index onward
    // (empty array if argc <= index)
    rest_args: { arity: 0, effects: E.GC, imms: ["index"] },
    // the arguments object (built from the raw argc/args)
    args_obj: { arity: 0, effects: E.THROW | E.GC },

    // --- for-in property iteration ------------------------------------------
    // the iterator value is an opaque non-ejsval; it must only be consumed
    // directly by the two ops below (never passed as a block argument)
    prop_iter_new: { arity: 1, effects: E.READ | E.THROW | E.GC },
    // produces an i1 (like to_boolean): true if a property was advanced to
    prop_iter_next: { arity: 1, effects: E.READ | E.WRITE | E.THROW | E.GC },
    prop_iter_current: { arity: 1, effects: E.READ | E.GC },

    // --- low tier ---------------------------------------------------------------
    has_tag: { arity: 1, effects: E.NONE, imms: ["tag"] },
    unbox_f64: { arity: 1, effects: E.NONE },
    box_f64: { arity: 1, effects: E.GC },
    f64_add: { arity: 2, effects: E.NONE },
    f64_sub: { arity: 2, effects: E.NONE },
    f64_mul: { arity: 2, effects: E.NONE },
    f64_div: { arity: 2, effects: E.NONE },
    f64_lt: { arity: 2, effects: E.NONE },
    call_runtime: { arity: -1, effects: GENERIC_OP, imms: ["name"] },

    // --- control flow --------------------------------------------------------------
    br: { arity: 0, effects: E.NONE, terminator: true },
    cond_br: { arity: 1, effects: E.NONE, terminator: true },
    return: { arity: 1, effects: E.NONE, terminator: true },
    throw: { arity: 1, effects: E.THROW, terminator: true },
    unreachable: { arity: 0, effects: E.NONE, terminator: true },

    // block parameter (not written by user code; created by the builder)
    blockparam: { arity: 0, effects: E.NONE },
};

export function opInfo(op) {
    let info = OPS[op];
    if (!info) throw new Error(`unknown EIR opcode '${op}'`);
    return info;
}

export function isTerminator(inst) {
    let info = opInfo(inst.op);
    if (info.terminator) return true;
    // any may-throw instruction with explicit control-flow targets (a
    // normal/unwind pair inside a protected region) terminates its block
    if (inst.targets && inst.targets.length > 0) return true;
    return false;
}

export function mayThrow(op) {
    return (opInfo(op).effects & Effect.THROW) !== 0;
}

export function isPure(op) {
    return opInfo(op).effects === Effect.NONE && !opInfo(op).terminator;
}
