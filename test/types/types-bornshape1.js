// born-with-shape probe (shapes-plan P4.4): statically-keyed literals
// lower to make_object_shaped, fenced constructor prefixes to the
// empty-shape-guarded fill_object_shaped — stdout must match node
// exactly, including enumeration order, `in` results, and growth past
// the born shape.  Stats line: bornShaped/ctorFills counts.
function Pt(x, y) { this.x = x; this.y = y; }
var p = new Pt(1, 2);
console.log(p.x + p.y);
console.log(Object.keys(p).join(","));

var lit = { a: 1, b: "s", c: true };
console.log(Object.keys(lit).join(","));
console.log(lit.a + lit.b);

p.tag = "t"; // grow past the born shape (a plain transition)
console.log(Object.keys(p).join(","));
console.log(("x" in p) + ":" + ("z" in p));

var mixed = new Pt("s", 2); // reprs differ from the candidate: still correct
console.log(mixed.x + mixed.y);
console.log(Object.keys(mixed).join(","));
