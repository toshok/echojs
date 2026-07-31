// the shapes-plan P4.6 accessor-inlining EVIDENCE probe (the extension
// was measured and DECLINED — see the plan's P4.6 entry).  defineProperty
// (not a getter literal — those are a maam NormalizeError) installs a
// proto getter; every p.len2 is an accessor dispatch through the generic
// get, and p.len2 correctly declines "no-field" (the accessor is not in
// the receiver's shape).  2026-07-24 numbers (M-series): 2.31s --types /
// 5.44s flag-off / 0.06s node; the same arithmetic through guarded slots
// runs 0.32s (~7x headroom).  Sound inlining needs proto-identity or
// proto-shape guards (a receiver has_shape proves nothing about the
// dictionary-mode proto carrying the getter) — a designed phase, not a
// measured extension.
function Pt(x, y) { this.x = x; this.y = y; }
Object.defineProperty(Pt.prototype, "len2", {
    get: function () { return this.x * this.x + this.y * this.y; }
});
function kern(p, n) {
    var s = 0;
    var i = 0;
    while (i < n) {
        s = s + p.len2;
        i = i + 1;
    }
    return s;
}
var out = 0;
var r = 0;
while (r < 20) {
    out = out + kern(new Pt(r, r + 1), 1000000);
    r = r + 1;
}
console.log(out);
