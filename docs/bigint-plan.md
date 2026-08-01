# bigint-plan: BigInt for echojs — buy vs build, and the plan

2026-07-31.  The last big language gate from language-P2/P3.  Indexed
on buy-vs-build per the goal; the answer is **buy — V8's standalone
bigint library**, vendored like our other C/C++ deps.

## The requirements that shape the choice

JS BigInt is not "a bignum": it has engine-shaped semantics —
- **two's-complement bitwise ops** (`& | ^ ~ << >>`) over negative
  values (most bignum libraries are sign-magnitude);
- `BigInt.asIntN/asUintN` (mod-2^n truncation);
- truncating division, spec `toString(radix 2–36)` / literal parsing;
- **exact** Number↔BigInt conversions;
- values are usually *small* (64–128 bits) — per-op overhead and
  allocation shape matter more than asymptotics.

echojs constraints: MIT license (LGPL is out — we ship static AOT
binaries); the GC prefers **inline limbs in a relocatable cell** (the
gc-plan mover) over malloc'd side-buffers with finalizers; the runtime
already links vendored C++ (double-conversion), so C++ is acceptable.

## The candidates

| | license | lang | JS semantics fit | GC fit | perf | notes |
|---|---|---|---|---|---|---|
| **V8 `src/bigint`** | BSD-3 | C++ | purpose-built: AsIntN/AsUintN, two's-complement bitwise, spec ToString/FromString | **caller-preallocates result digits** → limbs live inline in our GC cell, no finalizer | best available (Karatsuba/Toom/FFT mul, Burnikel-Ziegler/Barrett div, subquadratic toString) | designed standalone: its DEPS file *forbids* including anything outside `src/bigint`; internals include only std headers |
| libtommath | Unlicense | C | good: bitwise ops are two's-complement since 1.2; needs shims for asIntN + signed shifts | `mp_int` owns malloc'd digits → per-value finalizer | fine for small, quadratic toString | the safe fallback; battle-tested (tcl, dropbear) |
| libbf (Bellard) | MIT | C | float-centric representation; QuickJS itself **moved off it** for BigInt (Dec 2023) to a purpose-built implementation | context allocator | good asymptotics, heavier small-op | the QuickJS exodus is the tell |
| imath | MIT | C | sign-magnitude, no two's-complement helpers | malloc + finalizer | slow | too little for the shim work |
| GMP / mini-gmp | LGPL | C | — | — | fastest | license-blocked for static linking |
| build our own (QuickJS-style) | — | C | exact by construction | inline by construction | good small-op | ~2k lines; division and radix-conversion correctness are the long poles — conformance-first argues against |

## Decision: vendor V8's bigint library

It is the only candidate *designed for this exact job*: a JS engine's
BigInt core, deliberately factored for reuse (public API in
`bigint.h`, `Platform` hook for scratch allocation, interruption
support we can ignore).  The caller-preallocated `RWDigits` result
convention means our `EJSBigInt` GC cell owns its limbs inline —
relocatable under the planned mover, no finalizers, no double
allocation.  Two's-complement bitwise, AsIntN/AsUintN, and
FromString/ToString come semantics-correct out of the box.

Vendoring shape: `external-deps/v8-bigint/` — the ~18 files of
`src/bigint` at a pinned V8 commit + `LICENSE.v8` + a `regen.sh`
documenting the extraction (same pattern as external-deps/acorn), plus
a small `extern "C"` shim (`ejs-bigint-shim.cc`) exposing the dozen
entry points the C runtime needs.  Built as a cxx_library like
double-conversion.

Fallback recorded: if the extraction surprises (it shouldn't — the
DEPS wall is enforced in V8's CI), libtommath is the drop-in second
choice; the runtime-side API below is library-agnostic.

## Implementation plan

1. **Value representation** — new primitive tag `EJSVAL_TYPE_BIGINT`
   (0x0A, the symbol pattern): `EJSBigInt` GC cell = header +
   sign/length word + inline `digit_t limbs[]`.  GC scans it as a leaf
   (like primstrings); tag plumbing in ejsval.h, GC trace/typeof/
   ToEJSBool (0n falsy)/strict-eq.
2. **Runtime ops** (`runtime/ejs-bigint.c` + shim): add/sub/mul/div/
   mod/exp (div-by-zero RangeError), bitwise + shifts, unary -/~,
   compare (bigint↔bigint and mixed bigint↔number for relationals),
   loose-eq bigint↔number/string, ToString(radix)/FromString,
   Number↔BigInt exact conversions, `BigInt()` (ToPrimitive, integral
   checks), `asIntN`/`asUintN`, `BigInt.prototype`
   {toString,valueOf,[Symbol.toStringTag]}.
3. **Operator integration** — the generic binop paths in ejs-ops.c
   grow the ToNumeric split: arithmetic requires both-bigint
   (TypeError on mix), relationals/loose-eq allow mix, `+` checks
   string concat first.  The MAAM/typed fast paths guard on
   EJSVAL_IS_NUMBER and decline bigints to the generic path — no
   compiler changes.
4. **Compiler path** — parser gate deleted; acorn's `Literal.bigint`
   digit-string (NOT `.value` — the self-hosted parse feature-tests
   BigInt and yields null) lowers to a `%bigintFromString` intrinsic
   call.  `typeof` adds "bigint".  JSON.stringify throws TypeError.
5. **Tests + ratchet** — suite tests (arith/bitwise/compare/convert/
   errors), test262 built-ins/BigInt + language operator areas, full
   matrix, lane expectations regen.

Out of scope for the first pass: BigInt64Array/BigUint64Array (typed
arrays are their own P8.1-flagged rewrite), Intl-aware
toLocaleString.

## Implementation notes (as landed)

- Tag layout: `EJSVAL_TYPE_BIGINT = 0x09` slotted below OBJECT, which
  moved to 0x0A — OBJECT must stay the maximum tag (the 64-bit
  IS_OBJECT is a `>=` compare on the shifted tag, mirrored in
  lib/compiler.ts isObject).  BigInt cells ride the GC as leaves
  (`EJS_SCAN_TYPE_BIGINT`): every collector dispatch falls through, no
  finalizer, digits relocate with the cell.
- Optimizer soundness: the "result is always a Number" assumptions
  (cleanup.ts NUMBER_RESULT, optimize-guards NUMBER_RESULT_OPS) are
  false under BigInt.  Replaced with the one-proven-number-operand
  rule: mixing throws, so an op that *completes* with a number operand
  produced a number — this keeps every existing lattice fold and guard
  elision intact (verified by the eir unit suite) while making bigint
  results untyped.
- `++`/`--` lower as `to_numeric` (new op; bigints pass through,
  folds/identity rules mirror unary_plus) + add/sub carrying an
  `update` imm — the generic emission then calls
  `_ejs_op_add_update`/`_ejs_op_sub_update` (BigInt::add(x, 1n)
  instead of the mixed-operand TypeError) while typed paths and the
  optimizer see the ordinary add/sub.
- Literals: the parser adapter transmutes `Literal{bigint}` nodes into
  `%bigintFromLiteral("<digits>")` intrinsic calls (acorn's `.value`
  is null under the self-hosted parse — the digit string is the
  portable representation; separators stripped at runtime).
- `Number(bigint)` converts (the one ToNumber caller that does);
  implicit ToNumber throws; unary `+` throws; `>>>` throws.
- Known residue (test262 built-ins/BigInt at first landing: 69%):
  builtin length/name attribute gap (global, pre-existing),
  isConstructor-on-builtins (global), $262 realm tests.
