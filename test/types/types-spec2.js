// Phase 3.6 probe: CROSS-FUNCTION specialization (the hypot2-demo
// shape).  Both functions are module-local; hypot2's only calls live
// inside sum, whose slot store sits in the toplevel entry prefix (no
// CALL-effect instruction before it), so the cross-function loads
// rewrite too — including the one inside sum's own clone (the pass's
// fixpoint round).  specialized=2 specSites=4 (two toplevel sum calls,
// hypot2 in generic sum, hypot2 in sum$typed).
function hypot2(a, b) {
    return a * a + b * b;
}
function sum(n) {
    var total = 0;
    for (var i = 0; i < n; i = i + 1) total = total + hypot2(i, i + 1);
    return total;
}
console.log(sum(1000) + ":" + sum(2000));
