# compiler-P5 results — pass configuration: -O suites and -f/-fno- flags (P7.5)

2026-07-29.  The optimizer's configuration surface moves from ~20
`EJS_*` environment variables to a gcc/clang-style flag surface; env
reverts to what it should be — a short-lived debugging channel
(`EJS_FLAGS`), not the stable interface.

## What landed

### The pass registry (`lib/pass-config.ts`)

One table maps each canonical pass name to its `PassConfig` field, its
default at each -O level, and its help text.  `--help`'s pass section
and the `--print-passes` "effective configuration" listing are both
generated from the table, so they cannot drift from the truth.  Passes
read the resolved snapshot via `passes()` — never `process.env` (which
under the self-hosted runtime is a rebuild-the-whole-environment
getter; the `SinkFlags` snapshot in optimize.ts that motivated that
rule is now fed from the registry).  A side effect worth noting: the
per-instruction env reads in emit.ts (`env_load`/`env_store` inline
slot addressing checked `process.env` on every emitted instruction)
are now plain property reads.

### -O suites

- **-O0** — straight lowering: no EIR optimizer, LLVM O0.  The
  lowering/emission behaviors that were never `opt_level`-gated
  (born-shaped, shape guards under `--types`, promote, gc-frames,
  inline-alloc, inline-env-slots) stay on at every level — exactly
  today's -O0 behavior, preserved deliberately.
- **-O1** — the cheap always-sound intra-function tier: eir-opt,
  eir-cleanup, slot-cse, shaped-sink, args-sink, flow-sink.
- **-O2** (default) — adds the module-level tier: devirt, eir-spec,
  export-wrapper, ctor-sink, shape-fusion.  Byte-identical to the
  pre-P5 default pipeline (verified below).
- **-O3** — same EIR suite as -O2; only the LLVM pipeline runs
  `default<O3>`.  The EIR suite and the LLVM level stay one knob, with
  `-fllvm-opt=<0..3>` as the escape hatch decoupling the LLVM side.

### -f/-fno- per-pass flags

Applied after the suite in command-line order, last-wins (gcc
semantics).  Every `EJS_NO_X` maps 1:1 to `-fno-<kebab(x)>`:

| old env spelling | new flag |
|---|---|
| EJS_NO_EIR_OPT | -fno-eir-opt |
| EJS_NO_EIR_CLEANUP | -fno-eir-cleanup |
| EJS_NO_SLOT_CSE | -fno-slot-cse |
| EJS_NO_SHAPED_SINK | -fno-shaped-sink |
| EJS_NO_ARGS_SINK | -fno-args-sink |
| EJS_NO_FLOW_SINK | -fno-flow-sink |
| EJS_NO_CTOR_SINK | -fno-ctor-sink |
| EJS_NO_DEVIRT | -fno-devirt |
| EJS_NO_EIR_SPEC | -fno-eir-spec |
| EJS_NO_EXPORT_WRAPPER | -fno-export-wrapper |
| EJS_NO_SHAPE_GUARDS | -fno-shape-guards |
| EJS_NO_POLY_SHAPE_GUARDS | -fno-poly-shape-guards |
| EJS_NO_BORN_SHAPED | -fno-born-shaped |
| EJS_NO_SHAPE_FUSION | -fno-shape-fusion |
| EJS_NO_PROMOTE=a,b | -fno-promote=a,b (and blanket -fno-promote, new) |
| EJS_NO_GC_FRAMES | -fno-gc-frames |
| EJS_NO_INLINE_ALLOC | -fno-inline-alloc |
| EJS_NO_INLINE_ENV_SLOTS | -fno-inline-env-slots |
| EJS_EIR_LOWTIER=1 | -flowtier |

The env reads are deleted from the passes; the old spellings are inert.
`EJS_FLAGS` (tokenized as extra argv, applied after the real command
line so it wins, restricted to -O/-f tokens) is the single generic env
escape for bisecting inside harnesses that don't thread driver flags.
`test/tester.ts`'s `EJS_EXTRA_FLAGS` (a harness feature that already
threads argv) is unchanged and composes.

### Consumers ported

- `lib/eir/tests.ts`: the 12 bisect-flag tests use
  `withPassConfig({...}, () => ...)` instead of `process.env`
  mutation.
- `buck-test-lowtier.sh` / `//:test-eir-lowtier`: `-flowtier` instead
  of `EJS_EIR_LOWTIER=1`.
- CI needed no changes (no workflow set `EJS_*` compile-time vars;
  runtime knobs `EJS_GC_*`/`EJS_SHAPES*` are explicitly out of scope —
  they configure the produced binary's runtime, not the compile).

## The A/B gate (before deleting the env reads)

Run with the env fallback layer still in place (suite → env → flags),
one stage0 binary, comparing `--dump-after eir-opt` output:

- **env ≡ flag**: 35/35 pairs byte-identical across a targeted corpus
  (each pass exercised on files that trigger it; the three emit-level
  flags and eir-opt compared at the emitted-.ll level since they don't
  show in EIR dumps; lowtier compared on the pre-opt dump; EJS_FLAGS
  spelling included).
- **no default drift**: default-config dumps from the HEAD compiler
  (built in a worktree) vs this branch — byte-identical for flag-off
  and `--types` compiles across the corpus (types-bench2/3/4/5,
  modules1, types-flowsink1, types-ctorsink1).
- After deletion: defaults still byte-identical; `EJS_NO_*` verified
  inert.

## Gates

- tsc typecheck clean.
- test-eir: 216 pass + the same 11 compiler-P1.1 pins, nothing else.
- //:test-eir-lowtier green via `-flowtier`.
- Bootstrap matrix: stage0–3 (incl. the stage2/stage3 fixed point) +
  stage1-shapes-off all green, 424 pass / 21 xfail / 0 fail in every
  lane — identical to the phase-entry baseline.
- `--print-passes` / `--help` exercised; unknown-pass and bad
  `EJS_FLAGS` tokens fail loudly.

## Decisions and residue

- **-O1 semantics changed by design**: pre-P5, -O1 ran the full EIR
  optimizer (the only gate was `opt_level > 0`); it is now the
  intra-function tier.  -O2 is bit-for-bit the old default.
- `--types` stays a separate probe flag for now (the open question of
  folding it in as `-fmaam` is untouched; it defaults off, so it is
  not yet a suite member).
- Tuning knobs that were compile-time constants
  (`EJS_SHAPE_FIELD_CAP_MAX` in lower.ts) stay constants — they were
  never env vars despite the plan text; `-f<name>=<value>` machinery
  exists (`-fllvm-opt`, `-fno-promote=list`) when one needs to become
  configurable.
- New capability: blanket `-fno-promote` (the env spelling could only
  exclude by substring match).
