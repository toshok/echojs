// runtime-P2 probe: the escape-taint fence.  g is module-private and
// looks closed-world numeric — every context maam analyzed passes 3 —
// but one of its call sites is HOSTED in the exported f, whose
// activations can carry values the analysis never saw.  Under the
// analyzed constant maam prunes g's y>5 branch, so a trusted clone of
// g reached from f with an external 7 would run `s * 2` unguarded on
// the string "s" — the pre-existing cross-module miscompile the fence
// closes.  The init-time site (the toplevel g(3)) may still rewrite to
// the trusted clone; the site inside f stays generic (specFenced>=1).
function g(y) {
    var s = y > 5 ? "s" : y;
    return s * 2;
}
export function f(x) {
    return g(x);
}
console.log(g(3)); // init-time: analyzed, rewritable
console.log(f(3)); // f's own call is analyzed too — but f escapes
