# compiler-P1 results — optimizer residue (plans P5.4)

Phase record for compiler-plan.md's compiler-P1: the SSA cleanups,
the type lattice, module-slot load CSE, and direct-call
devirtualization.  Landed 2026-07-26 on `eir`.

## What landed

New passes (`lib/eir/cleanup.ts`, `lib/eir/devirt.ts`), wired in
`optimize.ts` / `integrate.ts`:

- **Type lattice** (`computeLattice`): trust-free flat lattice over
  boxed values — const kinds, fixed-result generic ops (`sub`/`mul`/
  `div`/`mod`/bit ops/`neg`/`unary_plus` → number; compares/`logical_not`/
  `instanceof`/`in`/`typeof_is` → boolean; allocation ops → object;
  `make_closure` → function; `typeof` → string; `add`'s operand rule),
  block-param meets iterated to fixpoint.  No oracle input, so it is
  sound (and fires) on flag-off compiles.
- **Cleanup fixpoint** (`cleanupFunction`, runs LAST in
  `optimizeFunction` — after the region passes, for foldUnboxOfBox's
  reason: folding arithmetic earlier perturbs the exact IR shapes the
  region matchers verify):
  - primitive-const folding, evaluated in the hosting engine (host and
    runtime implement the same ES semantics for primitive arithmetic).
    Fail-closed exclusions, each argued from a known host/runtime
    divergence: no folds that mint a string from non-strings (number
    formatting is the runtime's), no string relational compares
    (collation), no equality over `-0` (the runtime's strict_eq leads
    with a NaN-box tag compare, so `-0 === 0` is false there — the
    math2.js xfail — and a self-hosted compiler would fold it the
    runtime's way, breaking stage byte-identity);
  - `typeof` folds matching the RUNTIME's mapping (`null` → `"null"`,
    the documented quirk);
  - `typeof x === "T"` → `typeof_is` (the op existed in ops.ts but was
    never minted; the emit case now calls `_ejs_op_typeof_is_<T>`);
  - branch folding: cond_br on known-truthiness `to_boolean`
    (consts; lattice undefined/null falsy, object/function truthy),
    never-number `has_tag` FALSE folds, never-shaped `has_shape`
    FALSE folds; `to_boolean(logical_not x)` inverts the branch
    instead of calling `_ejs_op_not` + `_ejs_truthy`;
  - trivial block-param pruning (the SSA form of copy propagation);
  - **lattice-typed f64 lowering** — the "feed the low tier beyond
    the oracle" item: generic add/sub/mul/div both of whose operands
    are proven numbers compute unboxed with NO guard (`(a*1)+(b*1)`
    emits `f64_add` on any compile); `lt`/`gt` feed cond_br through
    `f64_lt` when the whole same-block lt/to_boolean/cond_br chain
    rewrites; `unary_plus` on a proven number is the identity.
  `EJS_NO_EIR_CLEANUP` bisects the whole group.
- **Module-slot load CSE** (`cseModuleSlotLoads`, runs BEFORE the
  region passes): block-local availability with store-to-load
  forwarding, killed at CALL-effect instructions; plus a dominance
  tier over STABLE %self slots — exactly one static store, sitting in
  the toplevel entry block.  Stability argument: the module init flag
  is set BEFORE the body runs (compiler.ts emitModuleResolution), so
  the toplevel executes at most once per process; a suspended init's
  remaining stores can only run after a callee returns, so a stable
  slot never changes during any activation.  Export-accessor setters
  count as stores, so an externally-writable export never qualifies.
  In the toplevel, every load the store comes-before folds to the
  stored VALUE; in other functions, dominated loads fold to their
  dominators.  `EJS_NO_SLOT_CSE` bisects.
- **Devirtualization** (`devirtualizeModule`, module pass in
  integrate.ts, runs AFTER specialization + ctor-sink so it never
  starves the strictly-better call_typed rewrite): SSA-visible `call`
  of a `make_closure` goes direct with the closure's env; a call
  through a single-store %self slot goes direct with an undefined env
  when the callee's %env param is entirely unused (load-observes-store
  proven via the specialize.ts toplevel-entry prefix rule or
  same-function dominance).  What invoke_closure does that a direct
  call skips: IS_FUNCTION (statically true) and the class-constructor
  TypeError — so any function whose closure could reach
  `set_constructor_kind_*` declines, and an unenumerable marking
  operand declines the whole module (fail closed).  `EJS_NO_DEVIRT`
  bisects.

## Fallout fixed en route

The phase's passes were the first to exercise several dormant paths;
four real pre-existing bugs fell out (the EIR-flush "27 latent bugs"
precedent, continued):

- **`Map.prototype.delete` was an unimplemented stub**
  (runtime/ejs-map.c `_ejs_map_delete`: spec steps in comments,
  `return _ejs_false;` since 2015).  cleanup.ts's CSE was the first
  compiler code to call Map.delete, so the SELF-HOSTED compiler's
  availability-kill silently kept stale entries and folded reloads
  across calls (stage1: updateassign1's compound-assign getter count,
  proxies, Symbol.hasInstance — 8 suite failures).  Implemented via
  the Set.delete pattern (key/value → NO_ITER_VALUE magic; set/get/
  size/iteration already skip empties).
- **ejs-llvm had no FP IRBuilder bindings beyond createFAdd**.
  Flag-off compiles never emitted f64 ops before the lattice pass, so
  a SELF-HOSTED compile that reached emit's f64 cases read a missing
  native method (boxed null) and threw "object not a function" —
  `createFSub`/`createFMul`/`createFDiv`/`createFCmpOLT` (+ atoms)
  added.  (Under node, node-llvm always had them — stage0 green while
  stage1 crashed, which is what made this hunt confusing.)
- **Emitter double-const cache `-0` collision** (compiler.ts
  `loadDoubleEjsValue`): cache key was `num_${n}` and `String(-0)` is
  `"0"` — a folded `-0` const emitted before a `+0` in the same
  function hijacked its cache slot (caught by the lowtier lane:
  `1/0` printed `-Infinity`).  And the first fix's guard
  (`n === 0 && 1/n < 0`) was itself disabled under self-host by the
  strict_eq `-0 === 0` tag-compare quirk — the final test is
  `1/n === -Infinity`, quirk-proof under both hosts.  cleanup.ts's
  `isNegZeroConst` uses the same form for the same reason.
- Two pre-existing guard-merge unit tests pinned the post-merge slow
  chain as fully generic; the slow `add` over (proven-number) mul
  results now lowers to f64, and the tests pin the new shape.

Soundness holes found by the gates and closed:

- has_tag FALSE-folding was removed from foldBranches: a boxed-repr
  slot_store's verifier proof IS a dominating has_tag=false fact, and
  folding the branch deleted the fact out from under the surviving
  store (the --types lane failed to compile every class file).
- Suspension awareness in CSE: a desugared generator body's
  activation can see the toplevel's remaining stores run mid-flight
  (create generator → drive it → store → resume), so functions
  containing generator_* runtime calls decline both the stable-slot
  dominance tier and the stable-survives-CALL exemption.

## Gate results (2026-07-28)

- `//:test-eir` unit tests green (17 new: cleanup folds, lattice
  lowering, trivial params, slot CSE incl. stability attacks and the
  generator-suspension decline, devirt incl. ctor-kind and env-use
  declines, bisect flags); `//:test-eir-lowtier` green.
- Full stage matrix green: `test-stage0` through `test-stage3` +
  `test-stage1-shapes-off` — the stage2/stage3 fixed point survives
  the compiler being optimized by (and running) the new passes.
  New suite test `map6.js` pins the Map.delete fix under every stage.
- `--types` diff lane: 474 files — 473 identical, **0 divergent**,
  1 N/A (the standing tester.js esprima gap), 0 compile failures.
- Toplevel shape-region merging (the shapes-P3 note this phase
  unblocks): `const p = {x:1,y:2}; console.log(p.x + p.y + p.x)`
  compiles to 2 has_shape guards with CSE vs 3 without
  (`p.x + p.x` after a store pair: 3 vs 4) — reloads no longer break
  receiver identity at toplevel.
- Self-compile telemetry (node-hosted `-d` over the whole compiler,
  38 modules): 1,483 call sites devirtualized (esprima 878,
  escodegen 225 — closure dispatch off the parser's hot paths),
  779 slot loads CSE'd, 580 lattice-typed ops lowered to f64,
  598 branches folded, 85 consts folded, 57 `typeof` tests rewritten
  to typeof_is, 45 trivial params pruned.
- Benchmarks:
  - **flag-off loop kernel** (`s = s + i*2 - i` ×50M, no --types):
    **0.09s vs 1.3s with cleanup off (~14×), 2.8× faster than node**
    (0.25s) — the lattice proves the loop-carried param is a number
    (init const + f64-add back-edge meet) and the whole loop computes
    unboxed with no guard, on a plain compile.
  - types-bench2 (--types): 0.21s vs 0.26s with the new passes off
    (~20%).
  - Self-hosted self-compile: 57.9s — inside the gc-P4 56–64s band;
    the new passes' compile-time cost is absorbed.
