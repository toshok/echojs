// born-with-shape wrong/edge cases (shapes-plan P4.4): the empty-shape
// guard and the runtime re-checks route every off-script construction
// through the sequential path with node-identical behavior.
function Pt(x, y) { this.x = x; this.y = y; }

// a reused non-empty receiver: the guard fails, sequential stores run
var reuse = { z: 9 };
Pt.call(reuse, 1, 2);
console.log(reuse.z + reuse.x + reuse.y);
console.log(Object.keys(reuse).join(","));

// `in` mid-construction cuts the fence at compile time
function Probe(x, y) {
  this.a = ("b" in this) ? 1 : 0;
  this.b = y;
}
var q = new Probe(5, 6);
console.log(q.a + ":" + q.b);

// a non-extensible receiver: the runtime re-check falls back, and the
// sequential [[Set]]s fail silently exactly like node (sloppy mode)
var frozen = Object.freeze({});
Pt.call(frozen, 7, 8);
console.log("" + ("x" in frozen));

// a proto-chain SETTER must intercept the batched assignment (the
// shaped_proto_intercepts fallback): hijack captures x, y stores own.
// (defineProperty, not an accessor literal — a getter/setter literal is
// a maam NormalizeError and would kill the oracle for the whole module,
// leaving nothing born-shaped to test.)
function P2(x, y) { this.x = x; this.y = y; }
P2.prototype = {};
Object.defineProperty(P2.prototype, "x", {
  set: function (v) { this.hijack = v; }
});
var h = new P2(1, 2);
console.log(h.hijack + ":" + h.x + ":" + h.y);

// a non-writable proto data property silently swallows the own-store
function P3(a, b) { this.a = a; this.b = b; }
P3.prototype = Object.freeze({ a: 99 });
var w = new P3(1, 2);
console.log(("a" in w) + ":" + w.a + ":" + w.b);
