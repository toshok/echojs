// the shapes-plan P4.3 object-model microbenchmark kernel — the twin of
// types-bench1: allocate N points through a monomorphic constructor and
// sum p.x*p.x + p.y*p.y, so the residual wall time is property access.
// The oracle types kern's parameter (and the module-local point) with the
// single terminal shape {x: num, y: num}; every p.x / p.y lowers to a
// has_shape diamond whose fast arm is a fixed-slot load.  Also serves as
// a probe: shapeGuards on the stats line counts the emitted diamonds.
function Point(x, y) {
    this.x = x;
    this.y = y;
}
function kern(p, n) {
    var s = 0;
    var i = 0;
    while (i < n) {
        s = s + p.x * p.x + p.y * p.y;
        i = i + 1;
    }
    return s;
}
function alloc(n) {
    var s = 0;
    var i = 0;
    while (i < n) {
        var p = new Point(i, i + 1);
        s = s + p.x + p.y;
        i = i + 1;
    }
    return s;
}
var out = 0;
var r = 0;
while (r < 20) {
    out = out + kern(new Point(3, 4), 1000000);
    out = out + alloc(200000);
    r = r + 1;
}
console.log(out);
