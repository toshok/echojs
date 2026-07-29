# compiler-plan: the EIR middle-end, optimizer residue, the TypeScript port, and driver ergonomics

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

- [x] **compiler-P1 — Optimizer residue.**  DONE 2026-07-28 —
      compiler-p1-results.md has the gate numbers.  The items from the
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
      Landed shape (`lib/eir/cleanup.ts`, `lib/eir/devirt.ts`):
      - **Type lattice**: trust-free flat lattice over boxed values
        (const kinds, fixed-result generic ops, allocation ops,
        `add`'s operand rule, block-param meets to fixpoint); sound
        with no oracle, so it fires on flag-off compiles too.
      - **Cleanup fixpoint** (runs LAST in optimizeFunction, after the
        region passes for the same reason foldUnboxOfBox does):
        primitive-const folding evaluated in the hosting engine
        (string-minting folds and string relationals excluded —
        number formatting and collation stay the runtime's; equality
        on `-0` declined — the runtime's tag-compare quirk (math2.js
        xfail) would otherwise diverge from the host AND break stage
        identity under self-compile); typeof folds matching the
        RUNTIME's mapping (null→"null" quirk included);
        `typeof x === "T"` → the (previously unminted) `typeof_is`
        op, now emitted via `_ejs_op_typeof_is_*`; cond_br folding on
        known truthiness / never-number `has_tag` / never-shaped
        `has_shape`; `to_boolean(logical_not x)` branch inversion;
        trivial block-param pruning (the SSA form of copy
        propagation); lattice-typed f64 lowering — generic
        add/sub/mul/div both of whose operands are proven numbers
        compute unboxed with NO guard, and lt/gt feed cond_br via
        f64_lt when the whole same-block chain rewrites.
        `EJS_NO_EIR_CLEANUP` bisects.
      - **Module-slot load CSE** (before the region passes — receiver
        identity is what lets toplevel shape regions merge):
        block-local availability with store-to-load forwarding,
        killed at CALL-effect instructions; plus a dominance tier for
        STABLE %self slots (exactly one static store, in the toplevel
        entry block — the init-flag-before-body ordering makes the
        toplevel run-once, so such a slot never changes during any
        activation; accessor setters count as stores, so externally
        writable exports never qualify).  `EJS_NO_SLOT_CSE` bisects.
      - **Devirtualization** (module pass, runs after specialization
        so it never starves the strictly-better call_typed rewrite):
        SSA-visible `call` of a `make_closure` goes direct with the
        closure's env; calls through a single-store %self slot go
        direct with an undefined env when the callee's %env param is
        unused (load-observes-store proven via the specialize.ts
        prefix rule or same-function dominance).  Functions whose
        closures could reach set_constructor_kind_* decline (the
        invoke_closure class-ctor TypeError must survive); an
        unenumerable marking operand declines the whole module.
        `EJS_NO_DEVIRT` bisects.
      - Fallout fixed en route (compiler-p1-results.md has the full
        stories): `Map.prototype.delete` was an unimplemented runtime
        stub (first compiler-side caller was this phase's CSE);
        ejs-llvm lacked every FP IRBuilder binding except createFAdd
        (flag-off compiles never emitted f64 before the lattice
        pass); the emitter's double-const cache collided `-0` with
        `+0` (and the fix's guard had to avoid the strict_eq `-0 ===
        0` tag-compare quirk to work under self-host); generator
        suspension makes "stable" slots unstable mid-activation —
        suspendable functions decline the CSE exemptions.
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
- [ ] **compiler-P5 — Pass-configuration ergonomics: -O suites and
      -f/-fno- flags.**  Env vars stop being the stable interface for
      configuring the optimizer; a gcc/clang-style flag surface
      replaces them, and env reverts to what it should be — a
      short-lived debugging channel.  Current state: `-O0`..`-O3`
      exist in the driver but only select the LLVM `default<O#>`
      pipeline plus one coarse `opt_level > 0` gate on the whole EIR
      optimizer; the real per-pass surface is ~20 `EJS_*` vars — the
      `EJS_NO_*` bisect family (EIR_CLEANUP, SLOT_CSE, DEVIRT,
      EIR_SPEC, SHAPE_GUARDS, POLY_SHAPE_GUARDS, BORN_SHAPED,
      SHAPE_FUSION, the `*_SINK` family, PROMOTE, INLINE_ALLOC,
      INLINE_ENV_SLOTS, GC_FRAMES), positive opt-ins
      (`EJS_EIR_LOWTIER`), and tuning knobs (`EJS_SHAPE_FIELD_CAP_MAX`,
      `EJS_SHAPE_NOMATCH`).  The shape:
      - **pass registry**: one table mapping canonical pass name →
        `CompilerOptions` field → default at each -O level; passes
        read options, never `process.env` (the per-run flag snapshot
        in `lib/eir/optimize.ts` generalizes into this).  `--help`
        and a `--print-passes` "effective configuration" listing are
        generated from the registry so it can't drift.
      - **-O suites**: `-O0` = straight lowering (no EIR optimizer,
        LLVM O0); `-O1` = the cheap always-sound tier (cleanup
        fixpoint, slot CSE, ...); `-O2` = today's full default.
        Decide whether `-O3` means anything yet or folds into `-O2`,
        and whether the EIR suite and the LLVM opt level stay one
        knob (probably yes, with an escape hatch for the LLVM side).
      - **-f\<pass\> / -fno-\<pass\>** per-pass overrides, applied
        after the suite in command-line order, last-wins — gcc
        semantics.  Tuning knobs become `-f<name>=<value>`.
      - **migration**: each `EJS_NO_X` maps 1:1 to a `-fno-x`; A/B
        gate that the old env spelling ≡ the new flag spelling, port
        `lib/eir/tests.ts` and the CI lanes off `process.env`
        mutation, then delete the env reads from the passes.  A
        single generic escape (`EJS_FLAGS=` injected as extra argv)
        can remain for bisecting inside harnesses that don't thread
        driver flags.
      - **open questions**: whether `--types` folds in as `-fmaam`
        (and eventually defaults on at `-O2`) or stays a separate
        probe flag; runtime-behavior knobs (`EJS_GC_*` etc.) are
        explicitly out of scope — they configure the produced
        binary's runtime, not the compile.
      Gates: bootstrap matrix green, stage identity, and the
      env≡flag A/B before the env reads are deleted.  Independent of
      compiler-P2..P4; can land any time.
