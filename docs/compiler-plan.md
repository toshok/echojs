# compiler-plan: the EIR middle-end, optimizer residue, and the TypeScript port

Bucket plan; the ordering spine lives in `docs/plans.md` (milestone
references look like `compiler-P1`).  Content moved here from the old
plans.md sections "Kill the legacy pipeline", "Optimization phase",
"TypeScript", and "Modules and linking".  See `EIRProposal.md` for the
IR design itself.

## Done: the EIR pipeline (history)

EIR (the block-argument SSA middle-end in `lib/eir/`) replaced the
AST+intrinsics pipeline outright, in four landed steps: (1) close the
per-function gaps — 424/424 candidate functions, zero fallbacks; (2)
desugars run pre-EIR (classes, destructuring, generators, spread,
meta-properties, hoisting) as pipeline-agnostic AST→AST passes; (3)
toplevel-as-EIR — whole modules lower as one EIR unit; (4) flip the
default and delete — new-cc, LambdaLift, the visitor middle-end and
eleven legacy-only desugars are gone (~9k lines), and a module that
doesn't lower is a compile error.  The stage2/stage3 byte-identity
fixed point runs under EIR self-compiles.  A pleasant side effect: the
work surfaced 27 latent compiler and runtime bugs, most with
regression tests.

The optimizer that grew on top (each with its own bucket where large):
guard-region folding/merging + raw f64 joins, function specialization
with a structural escape fence, env scalar replacement, object/array
literal sinking + iterator-wrapper folds (see sinking-plan for the
shaped-world continuation), shape-guard regions (see shapes-plan).

## Phases

- [ ] **compiler-P1 — Optimizer residue.**  The items from the
      original optimization list not owned by sinking-plan or
      shapes-plan:
      - the usual SSA passes where they pay: constant/copy
        propagation, redundant `to_boolean`/`typeof` elimination,
        direct-call devirtualization beyond siblings;
      - a type lattice over the currently-untyped `any` values,
        feeding the low-tier ops beyond what the oracle already
        types (TS annotations become a seed once compiler-P3 lands);
      - slot-load CSE for toplevel receivers (each module-slot access
        currently reloads, which blocks guard-region merging at
        toplevel — noted at shapes-P3).
- [ ] **compiler-P2 — TypeScript port of the compiler.**  The compiler
      converts from JS to TypeScript (largely done for lib/eir/ and
      lib/*.ts — the strict-TS conversion landed with the EIR work);
      remaining: the babel step in `//lib:generated` becomes tsc, and
      the residual JS entry points convert.  Sequenced before
      language-plan work (new-feature work is safer with types
      underneath it).
- [ ] **compiler-P3 — TypeScript as compiler input (tentative).**
      Slots in at the parser layer (type-stripping or a parser swap,
      coordinated with language-P2).  TS type annotations then seed
      the compiler-P1 type lattice.
- [ ] **compiler-P4 — Modules and linking.**  Static linking remains
      the regime (no dynamic loading planned):
      - reusable native modules from JS: a driver mode compiling a
        module to a `.a` plus a generated `.ejs` manifest (exports in
        slot order as the ABI, stably-named init function) so
        consumers link against compiled modules without recompiling
        them;
      - IR in the manifest: serialize the module's EIR so cross-module
        analysis and inlining through module boundaries work before —
        and instead of — any dynamic-loading story.
