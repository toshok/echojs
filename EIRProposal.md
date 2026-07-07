# EIR: a proposal for an EchoJS intermediate representation

This proposes replacing the compiler's AST+intrinsics middle-end with a
dedicated SSA IR ("EIR") that sits between the desugaring passes and LLVM
emission.  It is motivated by two concrete needs:

1. giving language-level optimizations a place to live (LLVM only sees
   opaque `_ejs_op_*` calls and can't reason about JS semantics), and
2. giving the abstract-interpretation static analysis effort a real
   dataflow substrate (CFG + SSA + effect annotations) instead of an AST.

## Why the current architecture fights us

Today's pipeline is:

    esprima AST
      → ~20 desugaring passes (AST → AST)
      → closure conversion (AST → AST + %intrinsic pseudo-calls)
      → LLVMIRVisitor (AST → LLVM IR, allocas everywhere)
      → llvm-as / opt -O2 / llc

Three structural problems fall out of this:

**The middle-end has no vocabulary of its own.**  Semantic operations are
encoded as `CallExpression`s with magic callee names (`%moduleGetSlot`,
`%slot`, `%makeClosure`, `%invokeClosure`, `%typeofIsObject`, ...) —
new-cc.js alone has ~50 `intrinsic(...)` construction sites.  Passes that
want to reason about these ops have to pattern-match call expressions
(`is_intrinsic(n.object, "%moduleGetExotic")`), and nothing checks that
an intrinsic's arguments are well-formed until LLVMIRVisitor throws (or
worse, silently miscompiles — several of the bugs fixed during the
bootstrap work were of exactly this shape: the for-of/destructuring pass
ordering bug, the module-slot layout bug, the `is32bit` truthiness bug).

**All dataflow is outsourced to mem2reg.**  Every local lives in an
entry-block `alloca` (`createAllocas` comments: "so the mem2reg opt pass
can regenerate the ssa form for us"), and every assignment is a
store/load pair.  That's fine as far as LLVM is concerned — clang does
the same — but it means *we* never hold an SSA view of the program.  By
the time SSA exists, the program is LLVM IR where `a + b` is an opaque
call to `_ejs_op_add` and a property access is `_ejs_object_getprop`.
LLVM can CSE neither, can't fold `typeof x === "string"` after a guard,
can't sink a boxing operation, can't stack-allocate a closure env that
doesn't escape.  Everything that requires knowing JS semantics is
currently optimized by nobody.

**The AST is a poor substrate for abstract interpretation.**  A
fixed-point dataflow analysis wants a CFG with explicit joins, values
with single definitions to attach lattice facts to, and effect summaries
per operation.  Deriving all of that on the fly from an AST (with
exitable-scope's implicit control flow, `arguments` aliasing, and
intrinsic-calls-as-expressions) means the analysis re-implements half a
compiler front-end before it can begin.

So: yes, I think this is the right move.  The honest caveat is that
alloca+mem2reg is *not* a performance problem by itself — the win is not
"skip mem2reg", it's everything a real IR unlocks: JS-aware optimization,
a shared substrate with the analysis work, verifiability, and the
deletion of the intrinsics-through-AST encoding.  Direct SSA emission is
then a pleasant side effect of already being in SSA.

## Design

### Shape

MLIR/Cranelift-flavored, not LLVM-flavored, in one specific way: **basic
block arguments instead of phi nodes**.  Block args are easier to build
directly from an AST, easier to verify, and dramatically nicer for an
abstract interpreter (a join point's values are just the block's
parameters — no "which predecessor am I" bookkeeping).  They translate to
LLVM phis mechanically at emission.

    module            := function*, module-metadata (imports, exports, slot table)
    function          := name, params, blocks, env-shape
    block             := label, block-args, instruction*, terminator
    instruction       := result? = opcode operand*, attributes
    terminator        := br / cond_br / switch / return / throw / unreachable
                         (call-like instructions may also terminate: see EH)

Values are typed.  The type lattice is EchoJS's, not LLVM's:

    any                        -- a boxed ejsval, contents unknown
      ├─ number  (⊇ int32)     -- still boxed; refinement facts
      ├─ string, symbol
      ├─ boolean, undefined, null
      ├─ object (optionally: object<shape-id>, array, function)
    raw types: f64, i32, b1, rawptr<env>, rawptr<obj>   -- unboxed, post-lowering

A value of type `number` is still an ejsval at the `any` level of the IR;
the type is a *fact*, not a representation.  Representation change is an
explicit instruction (`unbox_f64` / `box_f64`), introduced by lowering.

### Two tiers, one IR

Rather than two separate IRs, EIR has high-level and low-level opcodes in
one instruction set, and a lowering pass between them (SpiderMonkey
MIR/LIR and V8's ignition→turbofan pipelines both converged on something
similar; for a two-person project one IR with tiers is much cheaper).

**High tier** — one opcode per semantic operation the language has.
Everything the ~50 AST intrinsics encode today becomes a first-class,
verifiable instruction:

    %v = add %a, %b                      ; generic JS +, may throw (valueOf)
    %v = get_prop %obj, %key             ; may throw, reads heap
    set_prop %obj, %key, %v
    %v = get_prop_atom %obj, atom(length)
    %f = make_closure fn(@inner), %env
    %e = make_env 3, parent=%env0        ; env with 3 slots
    %v = env_load %e, slot(2)
    env_store %e, slot(2), %v
    %v = module_slot_load module(lib/consts), slot(46)
    module_slot_store module(...), slot(n), %v
    %v = call %callee, this=%t, args(%a, %b)
    %v = construct %callee, args(...)
    %b = to_boolean %v
    %b = typeof_is %v, "string"          ; pure
    %b = strict_eq %a, %b                ; pure
    %v = const ejsval(atom "Program")    ; pure
    ...

Every opcode carries an **effect signature** in a static table:
`{pure | reads-heap | writes-heap} × {may-throw} × {may-gc} × {may-call}`.
This table is the contract the optimizer *and* the abstract interpreter
both consume — it is the single most valuable artifact of the whole
design, and it's about 60 lines.

**Low tier** — what emission actually wants: tag tests, unboxing, raw
arithmetic, direct runtime calls:

    %t  = has_tag %v, double-tag         ; pure, b1
    %d  = unbox_f64 %v                   ; pure (requires proven tag)
    %r  = f64.add %d1, %d2
    %v2 = box_f64 %r
    %v  = call_runtime _ejs_op_add(%a, %b)   ; the fallback the high op lowers to

The lowering pass maps each high op to either (a) a guarded fast path +
runtime-call slow path, or (b) a plain runtime call — *informed by the
type facts on its operands*.  This is the hook where the abstract
interpreter pays rent: it runs on the high tier, refines operand types
(`%a: number`, `%b: number`), and lowering then emits `f64.add` with no
guards instead of `call _ejs_op_add`.  Today there is no place in the
pipeline where that transaction can even be expressed.

### Control flow and exceptions

All control flow is explicit edges between blocks.  `exitable-scope.js`'s
implicit break/continue/return-through-finally machinery disappears into
ordinary CFG construction (finally blocks are duplicated or dispatched at
lowering-from-AST time, exactly once, in one place).

Exceptions use LLVM's model, because we must emit it anyway: any
`may-throw` instruction inside a protected region becomes a terminator
with two successors:

    %v = invoke get_prop %obj, %key
             normal ^bb7(%v), unwind ^catch3(%exc)

Blocks reached by unwind edges are catch blocks; their block-arg is the
caught value.  Emission maps this 1:1 onto invoke/landingpad with the
existing EJS personality.  Outside protected regions, may-throw
instructions are plain instructions (unwinding propagates), same as
today.

### Functions, closures, environments

Closure conversion moves from an AST pass (new-cc, ~1500 lines of the
subtlest code in the compiler) into the AST→EIR lowering: scope
resolution assigns each binding to a param, an SSA local, or an env slot,
and emits `make_env`/`env_load`/`env_store` directly.  Because envs and
slots are first-class instructions with known effects, two optimizations
become straightforward EIR passes later:

- **env promotion**: a captured-but-never-mutated-after-capture slot's
  loads can be forwarded to the stored value; an env whose closure never
  escapes can be elided entirely (today every function with any capture
  allocates a GC'd env unconditionally);
- **direct calls**: `%f = make_closure fn(@inner), %e` followed by
  `call %f` can become a direct call to `@inner` with `%e` passed
  explicitly, skipping `_ejs_invoke_closure`'s dispatch.

The GC contract stays exactly as the runtime now guarantees it: ejsvals
and raw env/object pointers live in SSA values → machine registers/stack
slots, which the conservative scanner already handles (including interior
pointers, as of the recent GC work).  No stack maps needed.  The one rule
EIR must enforce (verifier-checked): a raw *derived* pointer may not be
live across a `may-gc` instruction unless the base is also live — which
the conservative scanner then makes safe.

### Textual format

Every function above implies it: EIR has a canonical textual form, parsed
and printed by the compiler.  This is load-bearing, not cosmetic — golden
tests for lowering, a `--emit-eir` flag for debugging, serialization for
the analysis tooling, and reduced repro cases all come from it.

Example — `function inc(x) { return x + 1; }` after lowering + analysis
proved nothing about `x`:

    fn @inc(%this: any, %x: any) -> any {
    ^entry:
        %c1 = const number(1)
        %r  = add %x, %c1            ; may-throw, may-gc
        return %r
    }

after the abstract interpreter proves `%x: number` at all call sites:

    fn @inc(%this: any, %x: any but-known number) -> any {
    ^entry:
        %d  = unbox_f64 %x
        %r  = f64.add %d, 1.0
        %v  = box_f64 %r
        return %v
    }

### Block-argument-driven specialization (basic block versioning)

Block arguments make one further strategy available that phi-form SSA
makes awkward: **specializing blocks on the types of their arguments**.
A block is a small function of its parameters; if analysis (or profiling)
shows a block is entered with `(number, string)` on one edge and
`(any, any)` on another, the lowering can *version* the block — clone it
per distinct argument-type tuple, wiring each predecessor edge to the
version matching the types it can prove it passes.  Inside a version,
the parameter types are facts, so guards disappear and unboxing floats
to the block entry.  This is Chevalier-Boisvert & Feeley's basic block
versioning (ECOOP'15), which gets most of the benefit of interprocedural
type inference at a fraction of the implementation cost, and it consumes
exactly the interface EIR already has: types attached to block
parameters, edges that pass arguments.  The static analysis can treat a
block as its unit of work — a lattice tuple in through the parameters,
facts out through the terminator's edges — and versioning is then a
lowering decision, not an analysis one.  (A version cap per block, ~4 in
the literature, bounds code growth.)

### SSA construction

Build SSA *during* AST→EIR lowering with the Braun/Buchwald/Hack
algorithm ("Simple and Efficient Construction of SSA Form", CC'13): local
value numbering per block + lazy block-arg insertion on demand, no
dominator computation, designed exactly for AST-to-SSA translation, and
small enough to implement in a few hundred lines of the JS we can
self-host.  (This matters: the compiler compiles itself, so the IR
implementation must be written in the subset of JS EchoJS handles, and
compile-time performance of the compiler is a user-visible cost.)

### What the abstract interpreter gets

- CFG with block args → textbook fixed-point iteration, join = block
  entry, no SSA-deconstruction shims;
- one definition per value → lattice facts keyed by value id, stored in a
  side table (the IR never mutates for analysis);
- the effect table → sound handling of calls/heap without re-deriving
  behavior from op names;
- module metadata (export slots, const-ness — gather-imports already
  computes `constval`) → interprocedural constants for free;
- the textual format → corpus capture and regression fixtures.

The contract between the two efforts is intentionally thin: the analysis
consumes high-tier EIR + the effect table, and produces a side table of
`value-id → lattice fact` (plus optionally `call-site → callee set`).
Lowering consumes that side table.  Neither needs the other to exist to
make progress: lowering without facts just always takes the generic
path, which is exactly today's behavior.

## What EIR replaces, and what it doesn't

Unchanged: esprima, all the *syntactic* desugaring passes (classes,
destructuring, for-of, generators, arguments, templates...), the runtime,
llc/linking.  Desugars are cheap, well-understood, and testable; EIR
should receive a maximally-desugared AST.

Replaced, eventually: `new-cc.js` (closure conversion → lowering),
`exitable-scope.js` (→ CFG construction), `compiler.js`'s LLVMIRVisitor
(→ a much smaller EIR→LLVM emitter: every EIR value is an LLVM value,
block args are phis, invoke edges are invokes — no allocas except the
few real ones: `arguments` objects, scratch areas).

## Migration plan

The bootstrap gives us an unusually strong safety net: 373 tests × 3
stages, plus the stage2≡stage3 fixed-point check, which catches
miscompiles of the compiler itself.  Use it.

1. **EIR core** (data structures, builder, verifier, printer/parser,
   effect table).  Pure addition; no behavior change.  Landable and
   testable standalone — and immediately usable by the analysis work.
2. **AST→EIR lowering + naive EIR→LLVM emission** behind a flag
   (`--ir`), initially only for functions using a whitelisted subset of
   constructs (fall back to the legacy path per-function otherwise).
   Success = test suite green with the flag on, then fixed point holds.
3. **Grow coverage** until the whitelist is "everything"; make `--ir`
   the default; keep legacy for one release as `--legacy-codegen`.
4. **Delete** new-cc/exitable-scope/LLVMIRVisitor; the AST intrinsics
   vocabulary disappears with them.
5. **Optimize** (now, not before): env promotion, direct calls,
   guard-informed lowering fed by the abstract interpreter, redundant
   box/unbox elimination, atom-keyed `get_prop_atom` ICs.

Phases 1–2 are the risky-design part and are deliberately boring in
behavior; phase 5 is where the payoff lives, and it only starts once the
suite + fixed point protect it.

## Risks, named

- **Semantics drift.**  LLVMIRVisitor encodes years of "oh right, JS
  does *that*".  Mitigation: per-function fallback during migration, the
  test suite, and porting visitor code case-by-case rather than
  rewriting from the spec.
- **Compiler self-hosting perf.**  An extra IR costs compile time;
  Braun-style construction and arena-ish (array-indexed, not
  pointer-soup) IR storage keep it linear.  Budget: self-compile time
  should stay within ~1.3× of today through phase 3, and win it back in
  phase 5 (less work for opt: we can likely drop `opt -O2` to `-O1` once
  we do our own scalar cleanup).
- **GC interactions.**  The conservative collector makes most of this a
  non-issue, but the derived-pointer-liveness rule must be in the
  verifier from day one, not discovered the way we discovered the
  register-scanning hole.
- **Two-team coupling.**  The analysis effort should consume EIR at
  phase 1; if the effect table or type lattice is wrong for them, we
  want that feedback before phase 3 freezes the design.

## Alternatives considered

- **Keep the AST, add annotations** (facts keyed by AST node): cheapest,
  but joins/loops have no natural representation, intrinsics stay
  stringly-typed, and emission stays alloca-shaped.  This is the status
  quo with more bookkeeping.
- **Emit better LLVM directly** (skip our own IR, build LLVM SSA with
  its own phi construction): removes mem2reg reliance but gives the
  analysis nothing (LLVM IR has erased JS semantics — `_ejs_op_add` is
  just a call), and ties every analysis/optimization to the llvm binding
  API.
- **CPS / sea-of-nodes**: more power than we need, much harder to
  implement, print, verify, and self-host.  Block-arg SSA is the
  sweet spot.
