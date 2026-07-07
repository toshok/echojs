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
    make_closure: { arity: 1, effects: E.GC, imms: ["fn"] },

    // --- modules -------------------------------------------------------------
    module_slot_load: { arity: 0, effects: E.READ, imms: ["module", "slot"] },
    module_slot_store: { arity: 1, effects: E.WRITE, imms: ["module", "slot"] },
    module_get_exotic: { arity: 0, effects: E.READ | E.GC, imms: ["module"] },

    // --- calls ----------------------------------------------------------------
    // call: operands = [callee, this, ...args]
    // construct: operands = [callee, ...args]
    // either may carry targets [normal, unwind] when inside a protected
    // region, in which case it terminates its block.
    call: { arity: -1, effects: GENERIC_OP, may_terminate: true },
    construct: { arity: -1, effects: GENERIC_OP, may_terminate: true },

    // --- allocation ------------------------------------------------------------
    make_array: { arity: -1, effects: E.GC | E.WRITE },
    // imms.keys: array of atom names, one per operand
    make_object: { arity: -1, effects: E.GC | E.WRITE, imms: ["keys"] },

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
    if (info.may_terminate && inst.targets && inst.targets.length > 0) return true;
    return false;
}

export function mayThrow(op) {
    return (opInfo(op).effects & Effect.THROW) !== 0;
}

export function isPure(op) {
    return opInfo(op).effects === Effect.NONE && !opInfo(op).terminator;
}
