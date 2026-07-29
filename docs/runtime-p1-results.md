# runtime-P1 results — pinned-bug burn-down (plans P7.1)

Phase record for runtime-plan.md's runtime-P1: the ten pinned runtime
bugs, fixed.  Landed 2026-07-29 on `eir`.  Every fix was verified
against node 22.4.0 on a direct repro before the suite gates ran.

## The ten, and what each turned out to be

1. **`typeof null` → `"object"`.**  Three coupled sites: the runtime
   mapping (`_ejs_op_typeof`), the compiler's constant fold
   (cleanup.ts `TYPEOF_OF_TAG`), and the `typeof x === "T"` peephole's
   runtime helpers.  `_ejs_op_typeof_is_object` now admits null — and
   excludes functions, which it had wrongly admitted (`typeof
   function(){} === "object"` was `true`; the helper never matched
   `_ejs_op_typeof`'s function-first ordering).  `typeof_is_null` is
   constant false.  typeof1.js un-pinned (`generator: none` dropped —
   node now agrees).

2. **`-0 === 0`.**  The strict_eq NaN-box TAG compare ran before the
   numeric compare, and ±0 are different bit patterns.  Numbers now
   compare first, by IEEE `==` (NaN and ±0 exact per spec).  The same
   tag-first flaw was latent in `_ejs_op_eq` (step 3's "same Type"),
   `SameValue` (whose step 6c had a typo making `Object.is(0,0)`
   FALSE), and `SameValueZero` (whose guard let a string/object pair
   fall into the string compare) — all four fixed.  cleanup.ts's
   equality-fold decline over `-0` consts (compiler-P1's workaround
   for the runtime quirk) is deleted; math2.js un-xfailed.

3. **`Math.round(-2.5)` → `-2`.**  C `round()` ties away from zero;
   ES ties toward +∞.  Now `floor(x + 0.5)` with the two exactness
   screens (|x| ≥ 2^52 already integral; |x| < 0.5 returns ±0 — the
   `0.49999999999999994` case where `x + 0.5` rounds to 1.0).

4. **`Number("  7  ")` → `7`.**  ToNumber's string path is a real
   StringToNumber now: trims the ES WhiteSpace ∪ LineTerminator set
   (on the UCS-2 code units), empty → 0, exactly-"Infinity" (strtod's
   "inf"/"nan" spellings rejected), 0x hex (digits only, no sign, no
   hex-float exponent), ES6 0b/0o, NaN on any non-ASCII unit.

5. **`-8 >>> 28` → `15`.**  The shift family cast the double operand
   straight to unsigned (UB; arm64 saturates negatives to 0).  All
   four shifts now go through ToInt32/ToUint32 — which also un-aborts
   their string/object operand paths.  `ToUint32` itself did the same
   UB cast and is now `(uint32_t)ToInt32`; bitand/bitor/bitnot moved
   from int64-truncating ToInteger to ToInt32.

6. **`1 + null` aborted.**  ToNumber had no null case (→ 0 now).  Add
   also tested the *original* operands for stringness rather than the
   ToPrimitive results, so `({}) + 1` numeric-added to NaN instead of
   concatenating — the ES string test is on lprim/rprim; fixed.

7. **`"a" * "b"` aborted.**  mult/div/mod were number-lhs-only with
   NOT_IMPLEMENTED arms; each is now just ToNumber both sides (with
   explicit evaluation order — sub too, whose C argument order was
   unsequenced).

8. **Uncaught throw out of a generator body.**  The desugar's outer
   catch rethrows on the generator's makecontext stack, and the
   unwinder walked off it into terminate.  `_ejs_generator_start` now
   invokes the body through `_ejs_invoke_closure_catch` (the runtime's
   existing landing-pad wrapper): the exception parks in the
   generator (`threw_out`), the context swaps back normally, and
   every resume site rethrows via `_ejs_generator_resume_result` — on
   the CALLER's stack.  `.next()` after the throw keeps answering
   `{ undefined, true }` per 25.3.3.3.  The four generator xfails
   (5/6/15/16) are a different debt (yield-expression sent values)
   and stay pinned.

9. **Sparse-array set NOT_IMPLEMENTED.**  The Arraylet type existed
   but nothing read or wrote one.  Implemented: fixed 512-slot
   chunk-aligned arraylets, sorted by start_idx (binary search;
   aligned chunks can't overlap), created on demand full of the same
   hole magic dense arrays use.  Get / GetOwnProperty / HasProperty /
   Set / Delete / DefineOwnProperty and length-shrink truncation all
   handle the sparse case; storage iteration (not length iteration)
   keeps `new Array(1e9)` O(present-elements).  sparsearray1.js
   un-xfailed.

10. **getOwnPropertyNames.**  Three defects: it filtered out
    non-enumerables (the pinned divergence — the filter belongs to
    Object.keys, not here), it threw-NOT_IMPLEMENTED on primitives
    (ES6 ToObject-coerces; null/undefined still TypeError), and it
    only walked the property map, so array / String-object index
    properties and `length` never appeared.  Index names now come
    first (OrdinaryOwnPropertyKeys order) via
    `_ejs_array_push_own_index_names` (arraylet-aware) or the
    String's primStr length, then `length`, then the map walk.

## Adjacent fixes the burn-down surfaced

- **C-side exception catchers leaked the gc-frame chain**
  (ejs-function.c / ejs-invoke-closure-catch.ll).  Emitted CATCH
  handlers re-link their own frame record as the chain head after an
  unwind; `_ejs_invoke_closure_catch` / `_ejs_invoke_func_catch` — C
  catchers with no frame record — left `_ejs_heap.gc_frame_head`
  pointing at the unwound (dead) emitted frames.  The generator fix
  made this reachable deterministically: the body's exception is
  caught on the generator stack, `pop_generator` parked the stale
  head, and the next minor walked dead frame records (segfault under
  EJS_GC_EVERY_N_ALLOC=7, no VERIFY needed; found by the stress sweep
  over the new repros).  Fix: the .ll wrappers became `*_inner` and C
  wrappers restore the saved chain head on the catch path — which
  also closes the same latent hazard at every existing C catcher
  (promise reactions, Map/Array.from ingestion, the iterator
  helpers).  Same lesson as runtime-P4's two finds: anything that
  depends on C-stack luck is a latent bug.
- **console.log formatting** (differential-lane fidelity, both
  pre-existing): `-0` prints as `-0` (node's inspect distinguishes
  it; ToString still collapses to "0" per spec), and strings nested
  in arrays print quoted (`[ 'a' ]`).  proxy6.js — pinned with
  `generator: none` precisely because of the unquoted format — is
  node-generated again.

## Pre-existing issues observed, NOT fixed here

- The PARANOID stress lane's generator failures (gc-gen*/generator*
  × EJS_GC_PARANOID) reproduce bit-for-bit on the phase-entry
  runtime — the recorded baseline set, unchanged.
- Under lldb's address layout, EJS_GC_EVERY_N_ALLOC=7 crashes during
  `_ejs_init` (xhr init setprop reads a 0xa7-poisoned cell) on the
  phase-entry runtime too — an environment-sensitive
  conservative-scan-luck use-after-free during init, recorded for a
  future stress pass.

## Un-pinned tests

typeof1 (generator:none dropped), math2 (xfail dropped), sparsearray1
(xfail dropped), proxy6 (generator:none dropped).  Still pinned, with
reasons unchanged: generator5/6/15/16 (yield sent values), math1 (ES6
Math functions), forin2/5, object6/7/9, and the rest of the xfail set
— none of them runtime-P1 items.

## Gates

- Matrix: test-eir-lowtier + stage0–3 (including the stage2/stage3
  byte-identity fixed point) + stage1-shapes-off — all green.
  Stage suites now 424 pass / 20 xfail / 0 fail (math2 and
  sparsearray1 un-xfailed and passing).
- test-eir: exactly the 11 pre-existing compiler-P1.1 failures
  (born-shaped test debt recorded at runtime-P4 close), nothing new.
- Stress sweep over every new-path repro (uncaught generator throw,
  sparse arrays, getOwnPropertyNames, the full value-op battery):
  EVERY_N_ALLOC 7/31/101 × VERIFY/PARANOID + NURSERY=off +
  COMPACT=off, all green.  The gc/generator test stress lane matches
  the phase-entry baseline failure set exactly (PARANOID generator
  items only, verified pre-existing by A/B against the phase-entry
  libecho).
- Direct repro battery (every bug above, constant and non-constant
  operand forms, plus Object.is/edge cases): byte-identical to node
  22.4.0 output.
