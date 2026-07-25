// the oracle sees TWO terminal shapes for sum's receiver — P2 {x,y} and
// P3 {z,x,y}, distinct classes whose shared fields sit at different
// slots — so p.x / p.y lower to the P4.6 2-way guard chain: each class
// takes its own fast arm, everything else shares one generic slow path.
function P2(x, y) { this.x = x; this.y = y; }
function P3(x, y, z) { this.z = z; this.x = x; this.y = y; }
export function sum(p) { return p.x + p.y; }
export function setx(p, v) { p.x = v; return p.x; }
export function mk2(x, y) { return new P2(x, y); }
export function mk3(x, y, z) { return new P3(x, y, z); }
console.log(sum(mk2(1, 2)));        // 3 — types the receiver with P2...
console.log(sum(mk3(10, 20, 5)));   // 30 — ...and with P3
console.log(setx(mk2(3, 4), 7));    // 7 (typed store, arm 1)
console.log(setx(mk3(5, 6, 7), 8)); // 8 (typed store, arm 2)
