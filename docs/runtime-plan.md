# runtime-plan: correctness burn-down and runtime features

Bucket plan; the ordering spine lives in `docs/plans.md` (milestone
references look like `runtime-P1`).  This bucket owns the pinned
runtime bugs (found by the differential harnesses and pinned by tests,
deliberately not fixed mid-phase) and runtime-side features that no
performance bucket owns.

## Phases

- [x] **runtime-P1 — Pinned-bug burn-down.**  All ten fixed; DONE
      2026-07-29 — docs/runtime-p1-results.md (each entry there
      records what the bug actually was):
      - `typeof null` → `"object"` (runtime + compiler fold +
        typeof_is helpers; typeof_is_object also stopped admitting
        functions).
      - `-0 === 0` → true (strict_eq compares numbers before the
        NaN-box tag; same flaw fixed in loose eq, SameValue — which
        returned false for `Object.is(0,0)` — and SameValueZero).
      - `Math.round` ties toward +∞.
      - `Number("  7  ")` → 7 (real StringToNumber: ES whitespace
        trim, "Infinity" only, 0x/0b/0o, empty → 0).
      - `-8 >>> 28` → 15 (shifts + ToUint32 had UB double→unsigned
        casts; shifts also coerce non-number operands now).
      - `1 + null` → 1 (ToNumber(null) = 0; add's string test moved
        to the ToPrimitive results).
      - `"a" * "b"` → NaN (mult/div/mod are ToNumber-both-sides).
      - Uncaught generator-body throw propagates to the caller
        (invoke_closure_catch at the body boundary; resume sites
        rethrow on the caller's stack).
      - Sparse-array element storage implemented (aligned 512-slot
        arraylets); sparsearray1.js un-xfailed.
      - `getOwnPropertyNames`: non-enumerables included, primitives
        ToObject-coerced, array/String index properties + `length`
        reported.
- [ ] **runtime-P2 — Export-boundary wrapper.**  Escaping entry points
      currently pin down specialization and unguarded-consumption
      opportunities (the compiler buckets decline them).  A generated
      boundary wrapper — generic signature outside, dispatching to
      specialized/trusting internals — lets module-internal call graphs
      optimize while exports keep full dynamic semantics.  (Referenced
      by maam-plan and sinking-plan as the standing follow-on.)
- [ ] **runtime-P3 — Value-based test harness.**  Test baselines are
      generated live by `node <test>` and are sensitive to node's
      console.log inspect-format drift (22.4 → 22.23 changed array
      formatting); CI pins node 22.4.0.  The durable fix asserts on
      values rather than inspect output.
- [x] **runtime-P4 — Collector structural refactor.**  Recorded during
      the gc-P2 debugging sessions, deliberately deferred while phases
      were landing: extract a cell-lifecycle module (alloc/free/color
      in one place), kill the mark-color mask flip in favor of explicit
      epochs, aligned LOS regions with O(log n) lookup (also unblocks
      raising the shaped-object field cap past 14), a real root
      registry API, one collection-policy function, and a file split
      (ejs-gc.c is ~3k lines).  Behavior-preserving; gated on the
      differential lanes.  DONE 2026-07-29 —
      docs/runtime-p4-results.md (the LOS lookup had already landed
      with gc-P4; the cap raise is shapes-plan business).  Flushed two
      pre-existing stack-luck hazards: orphaned old slot-storage envs
      (retirement now queues one precise scan) and the paranoid
      checker's self-scan of collector frames.
