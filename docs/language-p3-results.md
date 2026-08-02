# language-P3 results — feature implementation, payoff-ordered

Status: **complete** (2026-07-31).  Closes plans.md P8.3.

P8.2 left every stage-4 syntax feature *parsing* (acorn), with the
backend gating what it couldn't lower.  This phase deletes the gates in
the P8.1 payoff order: the whole modest-desugar tier, object
spread/rest, class bodies (fields, private members, static blocks) and
the async/await family.  Everything below is verified byte-identical to
node on dedicated test files (now in `test/`), and eight pre-existing
runtime/compiler bugs were flushed out and fixed on the way.

## What landed, by feature

Syntax (each deletes a parser-seam gate):

- **`**` / `**=`** — a real generic binary op end-to-end (estree →
  EIR `exp` op → `_ejs_op_exp`), not a `Math.pow` rewrite.  The shared
  `_ejs_number_exponentiate` implements the spec edges C `pow()` gets
  wrong (`(±1) ** ±Infinity` → NaN, exponent NaN → NaN) and `Math.pow`
  now uses it too (fixes `Math.pow(NaN, 0)` → 1).
- **`??`** — native short-circuit lowering in EIR's `logical()`: the
  nullish test is `loose_eq(l, null)` (exactly null/undefined, no
  valueOf), branch layout identical to `&&`/`||`.
- **Logical assignment `&&=` `||=` `??=`** — DesugarModernOps (new
  pass, runs first).  Identifier targets become `x op (x = v)`; member
  targets get an arrow-IIFE with base/key temps so everything evaluates
  once.  Short-circuit semantics (no store, rhs unevaluated) preserved.
- **Optional chaining `?.`** — DesugarModernOps.  Chains linearize into
  an arrow-IIFE statement sequence with per-link temps and
  `if (t == null) return;` short-circuits; method calls keep their
  receiver via `%f.call(%t, args)`; `super.x?.()` keeps `super` intact
  with `this` as receiver; `delete a?.b` short-circuits to `true`.
  Arrow bodies keep this/arguments/super lexical, so DesugarClasses
  still rewrites `super` inside.
- **Object spread** — DesugarSpread: consecutive plain properties stay
  native literal chunks (born-shaped path untouched); spread sources
  copy through `%copyDataProps` (`_ejs_copy_data_properties`, spec
  CopyDataProperties: own enumerable via Get, defined E/W/C); post-
  spread literal chunks fold in by descriptor (`%objectSpreadMerge`) so
  accessors survive as accessors.
- **Object rest** — DesugarDestructuring: `{a, [k]: v, ...r}` excludes
  the destructured keys via the same runtime helper; computed keys
  hoist into temps so the member read and the exclusion share one
  evaluation.
- **`catch { }`** — the adapter synthesizes a fresh unused `%`-binding.
- **Numeric separators, hashbang** — already worked via acorn;
  verified.
- **Class fields + private members + static blocks** — DesugarClasses
  grew a private-scope stack and a partition step:
  - public fields define through `%defineField`
    (`_ejs_define_field`: ToPropertyKey + CreateDataProperty E/W/C —
    a plain Put would fight `name`/`length` on static targets);
  - instance fields collect into a per-class `%initFields` closure
    called at the top of a base ctor or after each `super()` in a
    derived one (initializers therefore see class scope, not ctor
    params — the ctor-param-shadowing hazard is structurally avoided);
  - computed field keys evaluate once, at class-definition time,
    into iife-level temps;
  - private fields are per-name compiler-created WeakMaps in the class
    iife's scope; private methods/accessors are shared closures guarded
    by a per-class brand map.  The checked accessors are C runtime
    helpers (`_ejs_private_field_get/set/init`, `_ejs_private_brand_check`,
    `_ejs_private_has`) riding the weakmap inverted rep, throwing real
    TypeErrors ("Cannot read private member #x ...");
  - `#x in obj` desugars to `%privHas`;  compound assignment, update
    expressions, logical assignment and optional chains compose with
    private members (single-evaluation read-modify-write IIFEs);
  - static fields and `static {}` blocks run in declaration order in a
    `%initStatics` closure with `this` = the class.
- **async/await** — DesugarAsyncFunctions (new pass): an async function
  becomes a wrapper returning `__ejs_asyncDrive(function* (params) {
  body-with-await→yield }.apply(this, arguments))` — the existing
  coroutine generators are the suspension mechanism, promises the
  scheduling.  The driver (plain JS, parsed fresh per wrapper) steps
  the generator with next()/throw() through `Promise.resolve(...).then`.
  Arrows keep their params and `.call(this)` (lexical receiver).
  Async methods (class + object literal) work — P8.1's silent-miscompile
  hazard class is gone.  `for await` lowers to the async-iteration
  protocol: prefer `[Symbol.asyncIterator]()`, fall back to the sync
  iterator with each value awaited (async-from-sync).
  `Symbol.asyncIterator` added as a well-known symbol.

Still gated (clean located errors, recorded as P8-follow-ons): **async
generator functions** (`async function*` — needs yield-in-async
queuing), **BigInt literals**, **dynamic `import()`**, **`import.meta`**.
Top-level await errors in the desugar pass ("await is only valid in
async functions").

## Pre-existing bugs flushed out and fixed

Implementing on top of real machinery was, again, an effective fuzzer:

1. **`super.other()` dispatched to the wrong method** — DesugarClasses
   built the super reference from the *enclosing method's* key, so
   `super.foo()` inside `bar()` called `A.bar`.  Rebuilt from the
   callee's own property (computed keys included).  A silent
   miscompile for as long as classes have existed.
2. **DesugarGeneratorFunctions popped its %gen mapping for every
   function** — any non-generator closure nested in a generator body
   stripped the generator's own id and crashed the compile (or worse).
   Unshift/shift now pair on the function's own generator-ness.
3. **`Promise.all` never resolved** — the resolve-element function had
   been an `#if notyet` stub since 2015, and the remaining-count was a
   per-element snapshot.  Implemented with the spec's shared record (a
   1-element array all element closures point at).
4. **Array/String `OwnPropertyKeys` + descriptors** — the class ops
   were map-only, so Reflect-style consumers (and the new
   CopyDataProperties) missed every element; array index descriptors
   lacked enumerable/configurable (the "Object.keys(array) omits index
   keys" pin candidate from runtime-P3 — now fixed); string wrappers
   had no index descriptors at all; array holes reported as own
   properties.  Both classes now have real OwnPropertyKeys/
   GetOwnProperty specops.
5. **Array `DefineOwnProperty` clobbered on attribute-only defines** —
   `Object.freeze`/`seal` (now reaching elements via the fixed
   OwnPropertyKeys) wrote the descriptor's absent value into elements
   and set length to `ToUint32(undefined)` = 0 — template callsites
   went empty.  Value/length writes are now gated on the descriptor
   actually carrying a value.
6. **`ToEJSBool` aborted on symbols** (P8.1's single biggest crash
   signature, 293 JS-reachable aborts) and treated NaN as truthy
   (C `NaN != 0`).  Symbols are truthy, NaN is falsy.
7. **`String(symbol)` aborted** (`_ejs_String_impl`, another P8.1
   crash signature) — now returns the SymbolDescriptiveString.
8. **Class method/accessor attributes** — methods were defined
   non-writable/non-configurable (only `enumerable: false` was set),
   accessors non-configurable.  Now per spec (writable/configurable
   methods, configurable accessors) — this alone moved the
   class-elements probe area by ~28 points (propertyHelper).

## Compiler structure

- `lib/passes/desugar-modern-ops.ts` (new): optional chaining +
  logical assignment; runs first.
- `lib/passes/desugar-async-functions.ts` (new): async/await +
  `for await`; runs before DesugarClasses so async methods are plain
  by class-desugar time.
- DesugarClasses/DesugarSpread/DesugarDestructuring extended in place.
- estree dialect grew ChainExpression, PropertyDefinition, StaticBlock,
  PrivateIdentifier, AwaitExpression, `async`/`await`/`optional`
  fields; node-visitor dispatches them; EIR never sees any of them.
- New runtime intrinsics: `%copyDataProps`, `%objectSpreadMerge`,
  `%defineField`, `%makePrivateMap`, `%privFieldGet/Set/Init`,
  `%privBrandCheck`, `%privHas`.

## Tests

12 new suite files (node-generated expected-outs, value-based harness):
exponentiation1, nullish1, optchain1, logical-assign1, object-spread1,
object-rest1, catch-binding1, class-fields1, class-private1,
class-static1, async1, for-await1.

## test262 area probes (stage1, sloppy-only, same probe policy as P8.1)

Same runner/policy as P8.1 (sloppy-only, no `$262`); one probe-tooling
fix rode along: the async-test protocol's `doneprintHandle.js` reports
via `print()`, which echojs doesn't have — the runner now shims it, so
async completions are observable at all.

| area | now | note |
|---|---|---|
| expressions/exponentiation | 37/44 | rest = gated BigInt |
| expressions/coalesce | 22/24 | rest = TCO tests |
| expressions/optional-chaining | 25/38 | was 13 at P8.2; rest: eval carve-out, `async` -as-identifier parses, for-await-of interaction |
| expressions/logical-assignment | 40/78 | rest: NamedEvaluation (fn.name from assignment), propertyHelper attribute checks |
| expressions/object (whole area) | 612/1170 | spread landed; residue is methods/attributes/early-error breadth |
| statements/class/elements | 1095/1534 (71%) | class area was 26% at P8.1; residue: async-generator gate (258 compile), attribute breadth, 35 crashes |
| expressions/async-function | 71/93 | P8.1 area was 7–14% |
| statements/async-function | 60/74 | |
| expressions/async-arrow-function | 49/60 | |
| statements/for-await-of | 387/1234 | 781 fail-compile = the async-generator gate |

## Gates

- tsc clean (compiler + test tree)
- full matrix at stage0/1/2/3 + shapes-off: **438/20/0** per lane
  (426 + the 12 new suite tests; no regressions, no new xfails)
- test-eir 227 green (one matcher updated for the accessor descriptor's
  new `configurable` field) + lowtier OK
- every feature file diffed byte-identical against node during
  development (the suite's expected-outs are node-generated)

## Follow-ons recorded

- async generators (`async function*`) — the one gated family left
  from the payoff list's async tier; needs a queued driver.
- BigInt (value-representation decision), dynamic `import()` /
  `import.meta` (AOT module-story design) — unchanged from the P8.1
  list.
- Private accessor read with only a setter defined degrades to
  undefined after the brand check instead of throwing TypeError.
- `super()` captured inside an arrow in a derived ctor won't trigger
  field initialization (initializers inject after statement-position
  super() calls only).
- for-await doesn't run the iterator-close protocol on early exit
  (matches the existing for-of lowering).
- The runtime list from P8.1 (builtin property attributes,
  crash-to-TypeError, TypedArray rewrite, missing globals) remains the
  P8.4-adjacent runtime workstream.
