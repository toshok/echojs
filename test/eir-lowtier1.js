// Phase 2 low-tier probe.  With -flowtier in the compiler's
// environment these function bodies are swapped for hand-built EIR
// (has_tag guard -> unbox/f64 op/box fast path vs the generic slow path;
// see lib/eir/lowtier-probe.ts).  Without it they compile normally.
// Observable output must be identical either way.

function lowtier_add(a, b) { return a + b; }
function lowtier_sub(a, b) { return a - b; }
function lowtier_mul(a, b) { return a * b; }
function lowtier_div(a, b) { return a / b; }
function lowtier_lt(a, b) { return a < b; }

console.log(lowtier_add(2, 3)); // fast: 5
console.log(lowtier_add(0.5, 0.25)); // fast: 0.75
console.log(lowtier_add(NaN, 1)); // fast (NaN IS a number): NaN
console.log(lowtier_add(-0, 0)); // fast: 0
console.log(lowtier_add(2147483647, 1)); // fast: 2147483648
console.log(lowtier_add("a", "b")); // slow: ab
console.log(lowtier_add(2, "x")); // slow (mixed): 2x
console.log(lowtier_sub(5, 2)); // fast: 3
console.log(lowtier_sub(0.75, 0.5)); // fast: 0.25
console.log(lowtier_sub("5", 2)); // slow (string): 3
console.log(lowtier_mul(3, 4)); // fast: 12
console.log(lowtier_mul(-0.5, 4)); // fast: -2
console.log(lowtier_mul("3", 4)); // slow (string): 12
console.log(lowtier_div(1, 0)); // fast: Infinity (only a real fdiv does this)
console.log(lowtier_div(0, 0)); // fast: NaN
console.log(lowtier_div(7, 2)); // fast: 3.5
// (no slow-path div row: the runtime's generic _ejs_op_div aborts on
// non-number operands — ejs-ops.c:901, pre-existing gap.  Slow routing is
// the same parameterized diamond code path add/sub/mul exercise above.)
console.log(lowtier_lt(1, 2)); // fast: true
console.log(lowtier_lt(2, 1)); // fast: false
console.log(lowtier_lt(NaN, 1)); // fast: false
console.log(lowtier_lt(1, NaN)); // fast: false
console.log(lowtier_lt("a", "b")); // slow: true
