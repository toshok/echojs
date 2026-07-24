// the oracle here sees ONE terminal shape for sumxy's receiver — the
// module-local Point instances {x: num, y: num} — so p.x / p.y lower to
// has_shape diamonds against that shape.
function Point(x, y) { this.x = x; this.y = y; }
export function sumxy(p) { return p.x + p.y; }
export function mk(x, y) { return new Point(x, y); }
console.log(sumxy(mk(1, 2))); // the call that types the receiver
