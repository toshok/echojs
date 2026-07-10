# echojs — planned work

A living document; ordering within a section is roughly priority. See
`EIRProposal.md` for the IR design itself.

## Kill the legacy pipeline (done)

EIR (the block-argument SSA middle-end in `lib/eir/`) replaced the
AST+intrinsics pipeline (new-cc, LambdaLift, the statement/expression
half of LLVMIRVisitor). All four phases landed, and the legacy
middle-end has been deleted outright:

1. **Close the per-function gaps** — done. The self-hosted compiler
   lowers 424/424 candidate functions with zero fallbacks (try/finally,
   per-iteration loop environments, arrow lexical `this`, `arguments`,
   rest/default params, for-in, spread, and friends).
2. **Desugars run pre-EIR** — done. Classes, destructuring, generators
   (coroutine-style), spread, meta-properties and function-declaration
   hoisting are pipeline-agnostic AST→AST passes that run before EIR
   collection; EIR lowers their `%`-intrinsics via the table in
   `lib/eir/intrinsics.js`. Defaults/rest deliberately stay legacy-only:
   EIR's native handling is strictly better, and the passes die with the
   legacy pipeline.
3. **Toplevel-as-EIR** — done. Whole modules (toplevel statements,
   import/export init, every nested function) lower as one EIR unit;
   the legacy side keeps only module scaffolding. The toplevel-built
   compiler bootstraps and passes the full suite; labeled statements,
   object-literal accessors, tagged templates and new-with-spread all
   lower natively. Per-function candidate mode and its forwarding
   thunks are gone: a module the toplevel can't own falls back to the
   legacy pipeline whole, with a warning.
4. **Flip the default, then delete** — done. EIR is the only pipeline.
   A module that doesn't lower is a compile error; the remaining
   source-reachable unsupported constructs (`with`, delete-of-a-
   variable, computed accessor keys — which no pipeline ever compiled)
   are each asserted by a unit test, and everything else is guarded
   defensively behind the parser or the pre-EIR desugars. new-cc,
   LambdaLift, exitable-scope, the visitor middle-end and eleven
   legacy-only desugar passes are gone (~9k lines); LLVMIRVisitor keeps
   only the module scaffolding the EIR emitter borrows (module
   info/resolution, atoms, literal infrastructure). Export accessors
   are built directly as EIR. The stage2/stage3 byte-identity fixed
   point runs under EIR self-compiles.

A pleasant side effect: the EIR work surfaced 27 latent compiler and
runtime bugs, most with regression tests.

## Optimization phase (after the legacy kill)

Now that the IR is SSA with a declared effect table (`lib/eir/ops.js`),
a real optimizer becomes tractable. The guiding goal: **readable JS
idioms that overallocate should become zero-cost when semantics are
preserved** — destructuring returns, options objects, tuple-ish arrays.

- **Escape analysis + allocation sinking** — the big one. One analysis
  over the SSA graph (escaping positions: call/construct operands,
  `set_prop` values, returns/throws, module-slot stores; direct calls
  give cheap interprocedural edges), then sink in payoff order:
  1. `make_env` — every closure-bearing function allocates one, and
     per-iteration loop envs multiply that in hot loops; non-escaping
     closure environments scalar-replace into SSA values.
  2. `make_object`/`make_array` + own-key `get_prop_atom` folding —
     exactly the shape the destructuring desugar emits.
  3. A peephole recognizing the iterator-wrapper-over-array-literal
     pattern, rewriting to direct indexing so array patterns sink too.
  4. `rest_args`/`args_obj` when only indexed or `.length`'d.
- The usual SSA passes ride along cheaply once the framework exists:
  constant/copy propagation, DCE, redundant `to_boolean`/`typeof`
  elimination, direct-call devirtualization beyond siblings.
- A type lattice over the currently untyped (`any`) values, feeding the
  low-tier ops (`has_tag`/`unbox_f64`/`f64_*`) for unboxed arithmetic.
- **MAAM abstract-interpreter integration**: hook EIR up to the
  MAAM-based abstract interpreter being developed alongside this repo.
  It supplies types — including object shapes — which both seeds the
  type lattice above and strengthens allocation sinking (shape
  information makes own-key folding and escape reasoning sound in far
  more cases). The `ops.js` effect table is the declared contract for
  this consumer.

## TypeScript

1. **The compiler converts from JS to TypeScript.** Sequenced after the
   legacy pipeline is gone, remaining bugs are fixed, and test coverage
   grows (dedicated CI steps). Until then, avoid JS-idiom churn that a
   TS port would redo. The babel step in `//lib:generated` becomes tsc.
2. **TypeScript as compiler input — tentative.** Would slot in at the
   parser layer (type-stripping or a parser swap). If it happens, TS
   type annotations are a natural seed for the EIR type lattice above.

## Modules and linking

Static linking remains the regime (no dynamic loading planned).

- **Reusable native modules from JS**: a driver mode that compiles a
  module to a `.a` plus a generated `.ejs` manifest — exports in slot
  order as the ABI, stably-named init function — so consumers link
  against compiled modules without recompiling them.
- **IR in the manifest**: serialize the module's EIR into the manifest
  so cross-module static analysis and inlining through module
  boundaries work before (and instead of) any dynamic-loading story.

## Testing / CI

- The stage ladder (`//:test-eir`, `//:test-stage0..3`) IS the EIR
  matrix now; the `-ir`/`-legacy` target duplicates are gone.
- Broader coverage generally, as a prerequisite for the TS port.
- Computed accessor keys (`{ get [k]() {} }`) need an ejsval-key
  variant of the define-accessor runtime call to un-xfail object18.js.
