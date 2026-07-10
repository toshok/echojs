# echojs — planned work

A living document; ordering within a section is roughly priority. See
`EIRProposal.md` for the IR design itself.

## Kill the legacy pipeline (in progress)

EIR (the block-argument SSA middle-end in `lib/eir/`) replaces the
AST+intrinsics pipeline (new-cc, LambdaLift, the statement/expression
half of LLVMIRVisitor). The plan, most of which has landed:

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
3. **Toplevel-as-EIR** (`--ir-toplevel`, in progress) — whole modules
   (toplevel statements, import/export init, every nested function)
   lower as one EIR unit; the legacy side keeps only module scaffolding.
   The full test suite passes under the flag, and the compiler
   self-compiles with all 60 of its modules lowering whole. Open: the
   toplevel-built compiler must itself bootstrap (currently red in
   `--ir` mode), and the remaining per-module fallbacks — labeled
   statements, object-literal accessors, tagged templates,
   new-with-spread — become native EIR features. Fallback then becomes
   a compile error and the forwarding thunks die.
4. **Flip the default** — `--ir` becomes the pipeline, `--legacy` sticks
   around for one release, then new-cc/lambda-lift and the visitor
   middle-end (~7k lines) are deleted. LLVMIRVisitor keeps only the
   module scaffolding the EIR emitter borrows (module info/resolution,
   accessors, atom and literal infrastructure).

A pleasant side effect so far: the EIR work has surfaced 21 latent
compiler and runtime bugs, most with regression tests.

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

- Dedicated CI steps for the `--ir-toplevel` configuration (the
  buck-stage machinery already takes extra flags).
- Broader coverage generally, as a prerequisite for the TS port.
