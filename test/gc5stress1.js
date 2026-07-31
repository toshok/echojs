// embedded-slot stress: single-cell born-with-shape objects,
// growth past embedded capacity, ctor birth-capacity hints, dictionary
// migration out of embedded storage, f64 slots + repr flips, and
// old->young barrier traffic with the object as owner.  Run under
// EJS_GC_EVERY_N_ALLOC to force collections between every step.

// 1. ctor hint: first construct mishinted (bare cell + out-of-line),
//    subsequent constructs embedded.  p.x/p.y arithmetic keeps values hot.
function Point(x, y) {
    this.x = x;
    this.y = y;
}
var pts = [];
var s = 0;
for (var i = 0; i < 2000; i++) {
    var p = new Point(i, i + 0.5);
    pts.push(p);
    s += p.x + p.y;
}
console.log("s1", s);

// 2. growth past embedded capacity: literal born with 2 fields, then 6
//    more appended (out-of-line degrade), values must survive moves.
var grown = [];
for (var i = 0; i < 500; i++) {
    var o = { a: i, b: "b" + i };
    o.c = i * 2;
    o.d = { nested: i };
    o.e = "e" + i;
    o.f = i + 0.25;
    o.g = [i, i + 1];
    o.h = i % 2 === 0;
    grown.push(o);
}
var t = 0;
for (var i = 0; i < grown.length; i++) {
    var o = grown[i];
    t += o.a + o.c + o.f + o.d.nested + o.g[1] + (o.h ? 1 : 0);
}
console.log("s2", t, grown[123].b, grown[321].e);

// 3. old->young stores through shaped slots: long-lived receivers get
//    freshly allocated values written into existing slots (the
//    barrier-owner-flip path), across many collections.
var holders = [];
for (var i = 0; i < 100; i++) holders.push({ v: null, w: 0 });
for (var round = 0; round < 50; round++) {
    for (var i = 0; i < holders.length; i++) {
        holders[i].v = { fresh: round * 1000 + i };
        holders[i].w = round + i / 2;
    }
}
var u = 0;
for (var i = 0; i < holders.length; i++) u += holders[i].v.fresh + holders[i].w;
console.log("s3", u);

// 4. dictionary migration out of embedded storage: delete a field, then
//    keep using the object.
var migr = [];
for (var i = 0; i < 300; i++) {
    var m = { p: i, q: i * 3, r: "r" + i };
    if (i % 2 === 0) delete m.q;
    migr.push(m);
}
var v = 0;
for (var i = 0; i < migr.length; i++) {
    v += migr[i].p + (migr[i].q === undefined ? 0 : migr[i].q);
}
console.log("s4", v, migr[100].r, Object.keys(migr[0]).join(","), Object.keys(migr[1]).join(","));

// 5. repr flips in embedded slots: number slot takes a string, string
//    slot takes a number.
var flip = [];
for (var i = 0; i < 200; i++) {
    var f = { n: i, s: "x" + i };
    if (i % 3 === 0) { f.n = "now-a-string" + i; f.s = i * 7; }
    flip.push(f);
}
var w = "";
for (var i = 0; i < 5; i++) w += flip[i].n + "|" + flip[i].s + ";";
console.log("s5", w);

// 6. enumeration order + in-operator on embedded objects.
var e = { one: 1, two: 2, three: 3 };
var names = [];
for (var k in e) names.push(k);
console.log("s6", names.join("/"), "two" in e, "nope" in e);

// 7. ctor that installs a growing number of fields (hint too small on
//    later constructs).
function Growy(n) {
    this.base = n;
    if (n % 2 === 0) {
        this.extra1 = n + 1;
        this.extra2 = n + 2;
        this.extra3 = n + 3;
    }
}
var g = 0;
for (var i = 0; i < 400; i++) {
    var gr = new Growy(i);
    g += gr.base + (gr.extra3 === undefined ? 0 : gr.extra3);
}
console.log("s7", g);
