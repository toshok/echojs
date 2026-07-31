// the shapes-plan P4.6 array-element-shapes EVIDENCE probe (the
// extension was measured and DEFERRED — see the plan's P4.6 entry).
// a[j] is a computed member — no shape machinery applies (arrays are
// exotics outside shaped mode, maam smashes element types).  2026-07-24
// numbers (M-series, 20M reads): 0.57s --types / 1.38s flag-off /
// 0.06s node — real headroom, owned by a future typed-element-storage
// phase alongside the gc-plan work.
function kern(a, n) {
    var s = 0;
    var r = 0;
    while (r < n) {
        var j = 0;
        while (j < 64) {
            s = s + a[j];
            j = j + 1;
        }
        r = r + 1;
    }
    return s;
}
var arr = [];
var k = 0;
while (k < 64) {
    arr.push(k * 1.5);
    k = k + 1;
}
console.log(kern(arr, 312500));
