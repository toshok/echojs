# runtime-P2 results — the export-boundary wrapper + the escape-taint fence

Phase P7.2 (docs/plans.md), bucket runtime-plan.md.  Landed 2026-07-29
on branch `eir`.  The standing follow-on recorded by maam-plan (P3.6:
"the boundary-guard wrapper dispatching escaping entries to the clone
remains OPEN follow-on work") and sinking-plan, paid down — with one
deliberate design change from the sketch, and one pre-existing
soundness bug found and fixed on the way.

## Why the wrapper does NOT dispatch to the trusted clone

The plan sketch said "generic signature outside, dispatching to
specialized/trusting internals."  That is unsound, and the reason is
maam's value domain: **constant propagation** (numbers and strings are
tracked as constants up to a widening bound — src/lang/values.ts in
echojs-maam).  A claim maam makes about an escaping function's body can
be conditioned on the argument *constants* its analyzed call sites
passed — e.g. it prunes a `y > 5` branch entirely under `y = 3` — so
the claim can be false for an external call passing 7: a NUMBER.  A
boundary tag guard proves tags, not maam's entry state, so no has_tag
chain can license entering a trusted (unguarded, SpecMode) clone from
an un-analyzed caller.

What the wrapper dispatches to instead is an **untrusted clone**
(SpecMode.trusted = false, lower.ts):

- typed signature: f64 formals, boxed once at entry — `box_f64` is the
  optimizer's structural number proof;
- the body keeps the ordinary guarded diamonds; the diamond gate widens
  to assume-and-guard (`operandPlausiblyNumber`: only a POSITIVE
  non-number claim declines — nodes the oracle never saw, the norm for
  an exported-but-never-called-internally function, guard rather than
  decline);
- result stays boxed ("any"); no unguarded return unbox.

The optimizer then folds the formal-rooted diamonds *structurally* —
no oracle claim is ever consumed as fact.  The unit test asserts the
strong version: the optimized clone of the standard loop kernel carries
ZERO has_tag guards and raw f64 arithmetic, i.e. trusted-clone quality,
trust-free.  (One enabling fix: wrapper compiles run a second
optimizeModule pass after specialization — the loop-carried number
proofs only fit provenNumberAt's depth cap after cleanup's
trivial-param pruning, which runs at a pass's tail.  Wrapper-free
compiles skip it, byte-pure.)

The wrapper itself (specialize.ts installWrapper): a fresh entry block
takes over the calling-convention params; one `has_tag(number)` per
formal chains to a fast block (unbox all, `call_typed` the clone,
return its boxed result); any failure branches to the untouched
original entry — the generic body, full dynamic semantics.  Both
external callers (through the module slot) and internal ones (devirt
direct-calls the generic entry; LLVM inlines the prologue) reach the
same guards.  Candidacy: escaping closure + the static callee checks
(no rest/arguments/defaults, identifier params, ≥1 formal) + a payoff
check (the lowered clone must emit ≥1 diamond).  `EJS_NO_EXPORT_WRAPPER`
bisects.

## The pre-existing bug: trusted rewrites inside escaping functions

The same coverage argument turned up a live miscompile that PREDATES
this phase: call sites *hosted inside* an escaping function were being
rewritten to trusted clones.  An external caller enters the escaping
function with values the analysis never saw; those values flow to the
hosted site; the rewrite unboxes them unguarded against claims derived
from module-internal constants.  types-wrapperfence1 pins the exact
shape:

```js
function g(y) { var s = y > 5 ? "s" : y; return s * 2; }  // private
export function f(x) { return g(x); }
console.log(g(3)); console.log(f(3));                     // analyzed
```

maam prunes `y > 5` under the analyzed 3, types `s * 2` as num, g
trusted-clones, and the g-site inside f rewrote to `unbox_f64` of an
argument that is `"s"` when main calls `f(7)` — garbage where node
prints NaN.  (Verified live before the fix by the probe's stats:
`specialized=1` proves the claim existed.)

**The fix — the escape-taint fence** (specialize.ts): `tainted` = the
set of Funcs whose activations can observe un-analyzed values — the
escaping closures, closed under (a) callee-of-a-site-hosted-in-tainted
(arguments are tainted) and (b) created-inside-tainted (captured
environment is tainted).  Unknown-callee calls need no edge: a value
only becomes callable from tainted code by flowing there, which
already classifies its function as escaping.  Rules:

- an escaping function never gets a trusted clone (it takes the
  wrapper path);
- no site hosted in a tainted function is rewritten to a trusted
  clone (`specFenced` counts them; a clone with no coverable site is
  not minted);
- a tainted-but-non-escaping helper MAY still be trusted-cloned: the
  clone is entered only through rewritten sites in covered code, and
  every covered activation runs during module init — before any
  external caller can exist.  Its tainted (generic-entry) activations
  run the generic body.

Residual, documented: an import cycle can re-enter a partially
initialized module, so "covered code runs before external callers" has
that one corner; taint does not model it.  The fence has no off-switch
— it is a soundness fix, not an optimization.

## Gates

- **eir unit tests**: 213 pass (the 11 standing compiler-P1.1
  born-shaped pins remain, untouched by this phase).  New tests: the
  wrapper's structure and full guard-fold, decline paths (no payoff /
  env capture / frame ops), EJS_NO_EXPORT_WRAPPER, and the sharpened
  escaping-closures test (wrapped, never trusted, even under a lying
  stub oracle).
- **--types diff lane**: 476 files — 475 identical, 0 divergent, 1 N/A
  (tester.js, the standing esprima gap).  Includes the new probes.
- **probes** (test/types/README.md census updated):
  - types-wrapper1: `specWrapped=1`; numbers cross the boundary into
    the clone, a string and a missing arg fail the chain onto the
    generic body; flag-off/--types identical.
  - types-wrapperfence1: `specialized=1 specSites=1 specFenced=1`;
    `f(7)` → NaN, node-identical (the divergence this would have been
    is the pinned bug).
  - types-specescape1 (existing): now `specWrapped=1`, still identical
    and node-matching.
  - types-wrongoracle1 (existing): lib's exported `inc` now wrapped;
    `inc("x")` still routes generic → "x1".
- **types-bench5** (the headline): the types-bench1 workload with the
  kernel EXPORTED and every call crossing the module boundary.
  flag-off 0.34 s → **0.07 s user** with the wrapper (~4.9×), exact
  PARITY with types-bench1's closed-world trusted path on the same
  machine — the export boundary now costs one has_tag per formal per
  call.
- **bootstrap matrix**: recorded in the phase-close commit (flag-off
  compiles are byte-pure by construction — specialization only runs
  under --types, and the second optimizer pass only when a wrapper was
  minted).

## Follow-ons recorded

- wrappers / guarded per-site dispatch for tainted-called internal
  helpers (today they simply stay generic inside tainted hosts);
- a payoff gate that credits call-heavy bodies — a bare delegation
  export (`export function f(x) { return g(x); }`) currently declines
  its wrapper (`specRejected=1` in types-wrapperfence1);
- maam-side escape hardening (synthetic ⊤-argument entry contexts for
  escaping closures) would let the oracle itself account for external
  callers — heap flows included — and dissolve the import-cycle
  residual.
