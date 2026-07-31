// Phase 3.6 probe: the wrong-oracle discipline for specialization.  f
// LOOKS closed-world numerically (all direct calls pass numbers), but
// its closure also escapes as a call ARGUMENT — the structural escape
// analysis must reject the TRUSTED clone (specialized=0).  Since
// runtime-P2 the escapee gets the boundary wrapper instead
// (specWrapped=1): has_tag guards at the generic entry dispatch to a
// guarded trust-free clone, so via()'s string call fails the guard
// chain and runs the generic body.  Behavior must be identical to
// flag-off; note via() really does call f with a string, which the
// generic path handles (numeric string concat semantics preserved).
function f(n) {
    var s = 0;
    var i = 0;
    while (i < n) {
        s = s + i;
        i = i + 1;
    }
    return s;
}
function via(g, x) {
    return g(x);
}
var direct = f(10);
var indirect = via(f, 5);
var mixed = via(f, "3"); // a string reaches f only through the escape
console.log(direct + ":" + indirect + ":" + mixed);
