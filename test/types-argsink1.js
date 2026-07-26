// sinking-P3 probe: rest_args/args_obj length sinking
// (docs/sinking-plan.md).  Every line must match node exactly, with and
// without --types and under EJS_NO_ARGS_SINK.

function len0() { return arguments.length; }
function len2(a, b) { return arguments.length; }
function lenExpr(a) { return arguments.length - 1; }
console.log(len0(), len0(1), len2(), len2(1, 2, 3), lenExpr(1), lenExpr(1, 2, 3, 4));

function rl(a, ...r) { return r.length; }
console.log(rl(1), rl(1, 2), rl(1, 2, 3, 4));

// declining uses keep full semantics
function idx() { return arguments.length + ":" + arguments[0]; }
console.log(idx(), idx("x"));

function fwd() { return Array.prototype.slice.call(arguments).join(","); }
console.log(fwd(1, 2, 3));

function restAll(...r) { return r.length + ":" + r.join("|"); }
console.log(restAll(), restAll(1, 2));

// arrow captures the enclosing arguments (env escape declines the sink)
function arrowCapture(a) { var g = () => arguments.length; return g(); }
console.log(arrowCapture(1, 2, 3));

// generator rest resolves in the outer function and rides the env
function* gen(...r) { yield r.length; yield r[0]; }
var it = gen(7, 8);
console.log(it.next().value, it.next().value);
