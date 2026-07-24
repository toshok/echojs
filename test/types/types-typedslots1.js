// typed slots (shapes-plan P4.5): f64-repr slots are accessed RAW inside
// guard regions — slot_load produces a raw f64, slot_store consumes one,
// and the heterogeneous merge fuses shape + numeric regions so the kernel
// below runs one has_shape guard, raw loads, and raw arithmetic with one
// generic slow path.  The probe pins the semantics the raw flow must not
// disturb:
//   - the fused kernel on the matching shape (fast) and on repr-mismatched
//     / extra-field / dictionary-mode receivers (slow) — same values;
//   - bit-level observables through raw slot traffic: -0 (1/x sign), NaN,
//     Infinity survive store→load round trips;
//   - an f64-field store of a number takes the typed fast path; storing a
//     string into the same field is a repr TRANSITION (generic path) and
//     later reads guard-fail to the slow path — values stay node-identical;
//   - a boxed-field store of a non-number stays on its (boxed) fast path.
function Pt(x, y) {
    this.x = x;
    this.y = y;
}
function kern(p) {
    return p.x * p.x + p.y * p.y;
}
function getx(p) {
    return p.x;
}
function setx(p, v) {
    p.x = v;
    return p.x;
}
console.log(kern(new Pt(3, 4)));                 // fast: 25
console.log(getx({ x: "a", y: "b" }));           // repr mismatch: slow read, "a"
// (string * string is a standing runtime gap — ejs-ops.c _ejs_op_mult —
// so repr-mismatched receivers are exercised through reads, not kern)
var wide = { x: 1, y: 2, z: 3 };
console.log(kern(wide));                          // extra field: slow, 5
var del = { x: 5, y: 6 };
delete del.x;
del.x = 5;
console.log(kern(del));                           // dictionary mode: slow, 61

// bit-level observables through raw slot traffic
var q = new Pt(-0, 0 / 0);
console.log(1 / q.x);                             // -Infinity (the -0 survived)
console.log(q.y === q.y);                         // false (NaN survived)
console.log(setx(q, 1 / 0));                      // Infinity through the typed store
console.log(1 / setx(q, -0));                     // -Infinity through the typed store

// repr transition: the typed store's has_tag guard routes the string to
// the generic path, which transitions x to boxed; later typed reads
// guard-fail (shape changed) and stay correct
var t = new Pt(1, 2);
console.log(setx(t, "s"));                        // "s" (transition, generic)
console.log(t.x + t.y);                           // "s2" (guard-failing typed read)
console.log(setx(t, 9));                          // 9 (x now boxed-repr: generic again)
console.log(t.x + t.y);                           // 11

// a boxed field keeps its boxed fast path for non-numbers
function Tag(name, v) {
    this.name = name;
    this.v = v;
}
function rename(o, s) {
    o.name = s;
    return o.name;
}
var g = new Tag("a", 1);
console.log(rename(g, "b"));                      // boxed fast store
console.log(rename(g, 7));                        // number into boxed field: generic
console.log(g.name + ":" + g.v);
