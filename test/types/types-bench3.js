// the shapes-plan P4.6 polymorphic microbenchmark: the types-bench2
// kernel with TWO receiver classes alternating at one site ({x,y} and
// {z,x,y} — neither a transition-prefix of the other, and the shared
// fields at different slots).  The oracle reports both terminal shapes;
// the 2-way guard chain gives each class a fixed-slot fast arm.
// 2026-07-24 numbers (M-series): 0.31s with the chain — parity with the
// monomorphic twin — vs 1.67s declined (-fno-poly-shape-guards) and
// 3.64s flag-off.
function P2(x, y) { this.x = x; this.y = y; }
function P3(x, y, z) { this.z = z; this.x = x; this.y = y; }
function kern(p, n) {
    var s = 0;
    var i = 0;
    while (i < n) {
        s = s + p.x * p.x + p.y * p.y;
        i = i + 1;
    }
    return s;
}
var out = 0;
var r = 0;
while (r < 20) {
    out = out + kern(new P2(r, r + 1), 500000);
    out = out + kern(new P3(r + 2, r + 3, r), 500000);
    r = r + 1;
}
console.log(out);
