# selfhost-plan: runtime methods in lowered JS

Bucket plan; the ordering spine lives in `docs/plans.md` (milestone
references will look like `selfhost-P1`).  New bucket (2026-07-31).

## The idea

Runtime methods stop being opaque C functions and become **JS written
in a restricted dialect**, compiled by our own compiler into the
runtime — and, crucially, available to the optimizer as EIR.  An
`arr.map(f)` at a call site where the oracle knows the receiver's
shape and the callback's identity inlines map's loop, inlines `f` into
it, allocates the result born-shaped, and sinks it if it never
escapes.  "Array map turns into a for loop in the executable" is not a
metaphor; it is the existing P2–P6 passes composing, once they can see
through the runtime-call boundary.

Every mature engine converged on some form of this (SpiderMonkey
self-hosted JS, JSC `@`-intrinsic builtins, V8 via JS then Torque).
Two structural advantages they did not have:

- **We are AOT.**  V8's retreat from JS builtins was driven partly by
  boot-time compilation cost (snapshots).  Our builtins compile once,
  at build time, into the runtime library.  The objection dissolves.
- **The emitter enforces the GC contract.**  C runtime methods
  hand-maintain gc-frames, write barriers, and scan invariants — the
  source of runtime-P1's ten pinned bugs, the decade-old Map.delete
  stub, and the uninitialized-dense-elements crash (2026-07-30, the
  linux CI burn-down).  Self-hosted builtins get all of it from the
  emitter, the same way user code does.  A self-hosted splice
  *cannot* publish uninitialized memory; the emitter has no way to
  spell that.

## The dialect: JS + the compiler's own early vocabulary

The make-or-break design decision, and the one V8's history warns
about: unrestricted JS makes it too easy to write accidentally
observable or accidentally slow builtins.  The dialect is strict from
day one.

The central design point: **the `%intrinsic` namespace is not a new
surface to invent — it is the vocabulary the compiler itself already
generates early in lowering, made spellable in source.**  Where the
frontend lowers `a[i]` into guarded EIR that ends in specific ops and
targeted runtime calls, a builtin author writes those ops directly:
`%get_element_dense(a, i)`, `%array_length(a)`, `%has_shape(o, s)`,
`%make_array_shaped(n)`, `%call_function(f, thisArg, x, k, o)`.  One
vocabulary, two producers (the lowerer and the builtins author), one
consumer (EIR and everything downstream).  Consequences:

- The intrinsic inventory is *discovered, not designed*: audit what
  map/filter/indexOf/join/splice actually need, and it is largely ops
  EIR already has (slot ops, has_tag/has_shape, make_*_shaped,
  arg_len) plus a handful of new leaf ops for element access and bulk
  moves.
- **C does not disappear — it gets targeted.**  The end state still
  has real C intrinsics; they shrink from "all of Array.prototype" to
  leaf primitives with single obligations: bulk element move
  (memmove-shaped), string rope/flatten primitives, allocation,
  the genuinely-native kernel (GC, ejsval representation, pcre,
  ucontext generators, libuv, node-compat).  A C intrinsic that does
  one thing is auditable against one invariant; the last week of C
  bugs were all in functions juggling several.

Dialect rules (builtins mode in the frontend):

- Strict subset: no `with`, no `eval`, no implicit observable
  operations — a bare `a[i]` on an unproven receiver is a
  compile-time ERROR in builtins mode; the author chooses either the
  observable spec op (`%Get(o, k)`) or the unguarded fast op behind an
  explicit guard.  The spec's observable-operation *sequence* is
  normative (test262 checks the order of Gets in map); the dialect
  makes observability an explicit, reviewable choice.
- Cross-builtin calls bind to frozen intrinsic identities
  (`%ArrayPush`), never prototype lookups — a builtin's behavior can
  not be changed by monkey-patching another.
- Builtins compile through the SAME pipeline (EIR, optimizer, verifier)
  with a mode flag; no second compiler.

## Integrity: inlining under mutation

Call-site inlining of a builtin is devirtualization — the existing
identity guards answer "is this still %Array.prototype.map%".  The
outlined builtin must remain spec-correct for hostile receivers
(its own guards degrade to generic paths).  Cross-builtin frozen
references need an integrity epoch — the sinking-P2 accessor-epoch
pattern generalized: user code replacing a protected intrinsic bumps
the epoch; epoch-guarded fast paths fall back to the (still correct,
still self-hosted) outlined form.

## Delivery mechanics

- **Build**: builtins.js (dialect) compiles at runtime-build time into
  the runtime archive; the bootstrap ladder already proves the
  compiler-compiles-the-runtime's-dependencies shape.
- **Cross-module inlining**: compiler-P4 (plans P9.5, IR-in-manifest
  cross-module linking) is this epic's delivery vehicle — the builtins
  ship their EIR in the runtime's manifest exactly like any native
  module ships its IR, and user compiles import + inline it.  P9.5
  stops being a packaging nicety and becomes foundation.
- **Conformance**: language-P1/P4 (plans P8.1/P8.4, test262) is the
  safety net porting requires.  A port wave without test262 coverage
  of the ported methods is flying blind; sequence accordingly.
- **Size discipline**: outlined by default; inline only under oracle
  evidence (known receiver shape or known callback), with a size
  budget.  Inlining map everywhere is how executables bloat.

## Risks, named

- **Perf floor during transition**: the C versions have hand-tuned
  bulk paths.  Port method-by-method behind flags, A/B each (the
  standard phase discipline); bulk-copy intrinsics cover the
  memmove-shaped cases.
- **Debuggability**: a miscompiled builtin breaks everything
  downstream strangely (the stage1-red/stage0-green pain, amplified).
  Antidotes exist and are load-bearing: per-method flags, the
  differential harness, byte-identity stage gates.
- **Dialect creep**: every convenience admitted into builtins mode is
  a future audit surface.  New intrinsics and relaxations require the
  same evidence bar as optimizer passes.

## Phases

- [ ] **selfhost-P1 — Design + intrinsic inventory.**  The dialect
      spec (builtins-mode rules, error on implicit observable ops);
      audit 5 representative builtins (map, filter, indexOf, join,
      splice) against existing EIR ops; the delta = the new intrinsic
      list, each with its single C obligation stated.  The integrity-
      epoch design.  Deliverable: design doc + the inventory, no code.
- [ ] **selfhost-P2 — Plumbing + first builtin.**  Builtins mode in
      the frontend (%namespace parsing, dialect enforcement); build
      integration compiling builtins.js into the runtime archive; ONE
      simple builtin (Array.prototype.indexOf class) ported behind a
      flag, C version retained; gates: test262 subset for that method,
      full matrix, perf A/B parity-or-better.
- [ ] **selfhost-P3 — The payoff machinery.**  Builtin EIR in the
      manifest (with/after compiler-P4); call-site inlining under
      devirt identity + integrity epoch; benchmark gate: a map/filter
      kernel over a known-shape array reaches parity with the
      hand-written loop (the types-bench discipline).
- [ ] **selfhost-P4 — Port waves, payoff order.**  Array iteration
      kernels → String methods → Object plumbing → iterators →
      Map/Set/Promise machinery.  Each wave: test262-gated,
      differential-lane green, per-method A/B, C counterpart deleted
      only after its wave's gates hold.
- [ ] **selfhost-P5 — The targeted-C end state.**  Audit what C
      remains; every survivor is a leaf intrinsic with a stated
      single obligation; the runtime C inventory becomes a documented
      contract (what the emitter may assume of each).

## Interactions

- **After** P8.1/P8.4 (test262 harness) for any port wave; P1 design
  work can start any time.
- **After** P11/maam-P5 (self-hosted oracle) for the *shipped* payoff:
  call-site inlining under oracle evidence can be developed under the
  node-hosted driver, but a release binary that inlines builtins on
  type evidence needs the oracle in the binary.
- **With** compiler-P4 / plans P9.5 (IR-in-manifest) — shared
  foundation, likely co-developed in selfhost-P3.
- **Consumes** the entire P2–P6 optimization ladder; this is the epic
  that cashes those passes in at the library boundary.
- **Feeds** gc-plan: born-shaped result arrays and sinking apply to
  builtin-allocated results once builtins are EIR.
