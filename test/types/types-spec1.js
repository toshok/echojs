// Phase 3.6 probe: function specialization.  kernel is module-local
// (promoted slot, never exported), numeric-only, and too big for EIR
// inlining (multi-block loop) — the local-closed-world analysis clones
// it as f64(f64) and rewrites the exact-arity toplevel call sites to
// call_typed.  The extra-arg site stays on the generic path (still
// enumerated, still correct).  specialized=1 specSites=2.
function kernel(n) {
    var s = 0;
    var i = 0;
    while (i < n) {
        s = s + i * i - i / 2;
        i = i + 1;
    }
    return s;
}
var a = kernel(10);
var b = kernel(20);
var c = kernel(30, 99); // extra arg: generic site
console.log(a + ":" + b + ":" + c);
