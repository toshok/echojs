/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
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
} as const;

export interface OpInfo {
    // fixed operand count, or -1 for variadic
    arity: number;
    effects: number;
    // names of immediate (non-value) attributes the instruction carries
    imms?: readonly string[];
    // ends a block unconditionally
    terminator?: boolean;
    // terminates its block when it carries explicit normal/unwind targets
    may_terminate?: boolean;
    // typed signature (the low tier).  params: what each operand slot
    // accepts — "ejsval" (any boxed value; rejects f64/i1) or "f64".
    // result: the Inst.type this op produces.  Ops without a sig take and
    // produce boxed ejsvals ("any"); the verifier enforces the flow rules.
    sig?: { readonly params: readonly ("ejsval" | "f64")[]; readonly result: "any" | "f64" | "i1" };
}

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
    // add/sub carry an optional `update` imm from ++/-- lowering: the
    // generic emission then calls the increment-flavored runtime entry
    // (BigInt::add(x, 1n) instead of the mixed-operand TypeError); typed
    // paths and folds see the ordinary op
    add: { arity: 2, effects: GENERIC_OP, imms: ["update"] },
    sub: { arity: 2, effects: GENERIC_OP, imms: ["update"] },
    mul: { arity: 2, effects: GENERIC_OP },
    div: { arity: 2, effects: GENERIC_OP },
    mod: { arity: 2, effects: GENERIC_OP },
    exp: { arity: 2, effects: GENERIC_OP },
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
    // ToNumeric: like unary_plus but bigints pass through (++/--)
    to_numeric: { arity: 1, effects: GENERIC_OP },
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
    make_closure: { arity: 1, effects: E.GC, imms: ["fn", "name", "len"] },

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
    // imms.direct's typed sibling — a direct call to a
    // specialized clone (imms.fn) with an unboxed signature.  operands =
    // [env, ...args] where each arg slot's type must match the callee
    // Func.sig's formal ("f64" formals take raw f64 values); no `this`
    // (static callee checks exclude this/arguments/rest/defaults).  The
    // result type is the callee sig's result, stamped on the Inst by the
    // specialization pass and re-checked against the callee by
    // verifyModule (per-op sigs can't express callee-dependent typing).
    call_typed: { arity: -1, effects: GENERIC_OP, may_terminate: true, imms: ["fn"] },
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
    // computed-key accessor: operands [obj, key, accessor]; imms.kind is
    // "get" or "set" — each accessor defines separately (partial
    // descriptors merge in the runtime)
    define_accessor_computed: { arity: 3, effects: E.GC | E.WRITE, imms: ["kind"] },
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
    // the argument count from imms.index onward, as a boxed number:
    // max(argc - index, 0).  Minted only by the optimizer's args sinking
    // (a rest_args/args_obj whose only uses are `.length` reads folds to
    // this and the allocation drains).  Reads the immutable
    // calling-convention argc — effect NONE — but it IS a frame op:
    // never valid in specialized clones or across inlining.
    arg_len: { arity: 0, effects: E.NONE, imms: ["index"] },

    // --- for-in property iteration ------------------------------------------
    // the iterator value is an opaque non-ejsval; it must only be consumed
    // directly by the two ops below (never passed as a block argument)
    prop_iter_new: { arity: 1, effects: E.READ | E.THROW | E.GC },
    // produces an i1 (like to_boolean): true if a property was advanced to
    prop_iter_next: { arity: 1, effects: E.READ | E.WRITE | E.THROW | E.GC },
    prop_iter_current: { arity: 1, effects: E.READ | E.GC },

    // --- low tier ---------------------------------------------------------------
    // imms.tag: the runtime tag tested; only "number" is emitted today
    // (mirrors LLVMIRVisitor.isNumber, inheriting its per-target check)
    has_tag: { arity: 1, effects: E.NONE, imms: ["tag"], sig: { params: ["ejsval"], result: "i1" } },

    // --- shapes ----------------------------------------------
    // i1: does the operand's header shape index equal the module-interned
    // shape?  imms.shape keys Module.shapes (the ordered field list the
    // module interns at init, like atoms); the emitter folds the NaN-box
    // object check in exactly as isNumber backs has_tag.  Effect NONE — a
    // pure header compare.
    has_shape: { arity: 1, effects: E.NONE, imms: ["shape"], sig: { params: ["ejsval"], result: "i1" } },
    // fixed-slot access on a shape-guarded receiver.  imms.shape/imms.slot
    // name the guarded shape and the field index within it (the shape imm
    // repeats the guard's so the verifier compares instead of infers);
    // imms.repr is the FIELD's shape repr ("boxed" | "f64").  Typed
    // slots: repr:"f64" produces (slot_load) / consumes (slot_store) a RAW
    // f64 under the P2 typed-flow rules — sound because the guard proved
    // the field's repr, the shaped-world invariant "shape reprs describe
    // slot contents" says an f64 slot holds a number, and the NaN-box
    // stores doubles raw, so the 8 bytes at the slot ARE the double.
    // slot_load's result type is repr-dependent (f64 for "f64", boxed
    // otherwise) — stamped by lowering and re-checked by the verifier,
    // the call_typed precedent for typing a per-op table can't express.
    // The verifier requires every slot op to be
    // dominated by an un-killed has_shape fact on the same value for the
    // same shape (see the effect-kill inventory in verifier.ts) — without
    // it a stale shape would make the slot addressing itself unsafe (the
    // storage word is a MAP pointer in dictionary mode).  slot_store
    // proves the stored value's repr matches the field: an f64 store takes
    // a raw f64 operand (a number by construction — the type system IS the
    // proof); a boxed store still requires a dominating has_tag=false fact
    // on the stored value, so the store provably never needs a repr
    // transition.
    slot_load: { arity: 1, effects: E.READ, imms: ["shape", "slot", "repr"] },
    slot_store: { arity: 2, effects: E.WRITE, imms: ["shape", "slot", "repr"] },
    // --- born with their shape -----------------------------
    // a statically-keyed object literal, allocated + installed in one
    // runtime call: operands are the initial field values in imms.shape's
    // field order.  The runtime re-derives the true shape from the actual
    // values (a wrong static repr can never mint a lying shape) and falls
    // back to today's sequential generic sets whenever the shaped fast
    // path doesn't apply — same GC|WRITE effect envelope as make_object.
    make_object_shaped: { arity: -1, effects: E.GC | E.WRITE, imms: ["shape"] },
    // a fenced constructor's straight-line this-store prefix, batched onto
    // the construct-allocated receiver: operands are [this, values...].
    // Only valid behind a passed has_shape(this, "") — the empty-shape
    // guard — which the verifier enforces via the same un-killed-fact
    // discipline as slot ops (a non-empty or dictionary-mode receiver
    // must take the sequential slow arm, where mid-construction
    // observables behave identically).
    fill_object_shaped: { arity: -1, effects: E.GC | E.WRITE, imms: ["shape"] },
    // i1: is the runtime's accessor epoch still zero — i.e. has NO user
    // code installed anything that could intercept a [[Set]] through a
    // fresh object's prototype chain (accessor property, non-writable
    // data property, prototype swap; see _ejs_accessor_epoch in
    // ejs-object.h)?  Minted only by the optimizer's constructor-result
    // sinking, guarding a virtualized (allocation-free) construct
    // against the interception the deleted stores could have met.  One
    // global load + compare; READ because the global is mutable.
    epoch_check: { arity: 0, effects: E.READ, sig: { params: [], result: "i1" } },
    // a raw f64 constant (imms.value).  minted only by the optimizer
    // (rawJoinParams' const-number edge roots) and the specialization
    // pass; lowering itself always emits boxed `const` numbers.
    f64_const: { arity: 0, effects: E.NONE, imms: ["value"], sig: { params: [], result: "f64" } },
    unbox_f64: { arity: 1, effects: E.NONE, sig: { params: ["ejsval"], result: "f64" } },
    box_f64: { arity: 1, effects: E.GC, sig: { params: ["f64"], result: "any" } },
    f64_add: { arity: 2, effects: E.NONE, sig: { params: ["f64", "f64"], result: "f64" } },
    f64_sub: { arity: 2, effects: E.NONE, sig: { params: ["f64", "f64"], result: "f64" } },
    f64_mul: { arity: 2, effects: E.NONE, sig: { params: ["f64", "f64"], result: "f64" } },
    f64_div: { arity: 2, effects: E.NONE, sig: { params: ["f64", "f64"], result: "f64" } },
    f64_lt: { arity: 2, effects: E.NONE, sig: { params: ["f64", "f64"], result: "i1" } },
    call_runtime: { arity: -1, effects: GENERIC_OP, imms: ["name"] },

    // --- control flow --------------------------------------------------------------
    br: { arity: 0, effects: E.NONE, terminator: true },
    cond_br: { arity: 1, effects: E.NONE, terminator: true },
    return: { arity: 1, effects: E.NONE, terminator: true },
    throw: { arity: 1, effects: E.THROW, terminator: true },
    unreachable: { arity: 0, effects: E.NONE, terminator: true },

    // block parameter (not written by user code; created by the builder)
    blockparam: { arity: 0, effects: E.NONE },
} as const satisfies Record<string, OpInfo>;

export type OpName = keyof typeof OPS;

export function isOpName(op: string): op is OpName {
    return Object.prototype.hasOwnProperty.call(OPS, op);
}

export function opInfo(op: string): OpInfo {
    if (!isOpName(op)) throw new Error(`unknown EIR opcode '${op}'`);
    return OPS[op];
}

// the structural slice of Inst that terminator-ness depends on (ir.ts
// imports from here, so this module can't import Inst without a cycle)
export interface InstLike {
    op: string;
    targets?: readonly object[] | null;
}

export function isTerminator(inst: InstLike): boolean {
    let info = opInfo(inst.op);
    if (info.terminator) return true;
    // any may-throw instruction with explicit control-flow targets (a
    // normal/unwind pair inside a protected region) terminates its block
    if (inst.targets && inst.targets.length > 0) return true;
    return false;
}

export function mayThrow(op: string): boolean {
    return (opInfo(op).effects & Effect.THROW) !== 0;
}

export function isPure(op: string): boolean {
    return opInfo(op).effects === Effect.NONE && !opInfo(op).terminator;
}
