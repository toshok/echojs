# runtime-plan: correctness burn-down and runtime features

Bucket plan; the ordering spine lives in `docs/plans.md` (milestone
references look like `runtime-P1`).  This bucket owns the pinned
runtime bugs (found by the differential harnesses and pinned by tests,
deliberately not fixed mid-phase) and runtime-side features that no
performance bucket owns.

## Phases

- [ ] **runtime-P1 — Pinned-bug burn-down.**  Each has a pinning test
      or a recorded repro; fix in any order, keeping the differential
      lanes green:
      - `typeof null` → `"null"` (should be `"object"`).
      - `-0 === 0` evaluates false (should be true).
      - `Math.round(-2.5)` → `-3` (should be `-2`; ties round toward
        +∞).
      - `Number("  7  ")` → `NaN` (whitespace should trim).
      - `-8 >>> 28` → `0` (should be `15`; unsigned-shift coercion).
      - `1 + null` aborts in the runtime (ejs-ops.c generic add) —
        should evaluate to `1`.
      - `"a" * "b"` aborts (`_ejs_op_mult`) — should be `NaN`.  Repro
        note: probes must exercise repr-mismatch via reads until fixed.
      - An uncaught throw out of a generator body aborts (the desugar's
        outer catch rethrows on the generator stack and the unwinder
        walks off the makecontext frame; node prints the error in the
        caller).  Exceptions/coroutine interaction needs an owner.
      - Sparse-array `set` through the exotic path is NOT_IMPLEMENTED
        (`new Array(N)` + `arr[i] =` aborts) — tests avoid the pattern
        today.
      - `getOwnPropertyNames` on non-enumerable-bearing objects
        diverges from node (pre-existing, mode-independent).
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
- [ ] **runtime-P4 — Collector structural refactor.**  Recorded during
      the gc-P2 debugging sessions, deliberately deferred while phases
      were landing: extract a cell-lifecycle module (alloc/free/color
      in one place), kill the mark-color mask flip in favor of explicit
      epochs, aligned LOS regions with O(log n) lookup (also unblocks
      raising the shaped-object field cap past 14), a real root
      registry API, one collection-policy function, and a file split
      (ejs-gc.c is ~3k lines).  Behavior-preserving; gated on the
      differential lanes.
