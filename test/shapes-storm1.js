// slot-storage stress: transition churn across every shaped-mode
// boundary — adds, repr flips, deletes, attribute/accessor migration,
// symbol and index keys, freeze/seal, enumeration order, `in` checks.

// plain construction + repr flips
var objs = [];
for (var i = 0; i < 200; i++) {
    var o = { a: i, b: "s" + i };
    o.c = i * 1.5;
    o.d = i % 2 === 0 ? i : "odd" + i; // alternating repr chains
    o.c = "now-a-string-" + i;         // repr flip after the fact
    objs.push(o);
}
var sum = 0;
for (var i = 0; i < 200; i++) {
    sum += objs[i].a;
    sum += objs[i].c.length;
}
console.log("sum", sum);

// delete drops to dictionary; re-add after delete
var del = { x: 1, y: 2, z: 3 };
delete del.y;
console.log("del keys", Object.keys(del).join(","));
del.y = 42;
del.w = 5;
console.log("del keys2", Object.keys(del).join(","), del.y, "y" in del, "v" in del);

// non-default attributes migrate
var attr = { p: 1, q: 2 };
Object.defineProperty(attr, "r", { value: 3, enumerable: false });
console.log("attr keys", Object.keys(attr).join(","), attr.r);
// (getOwnPropertyNames on all-enumerable objects only: echojs filters
// non-enumerable names, mode-independently)
console.log("attr names", Object.getOwnPropertyNames({ p: 1, q: 2 }).join(","));

// plain defineProperty with default attrs stays shaped
var dp = {};
Object.defineProperty(dp, "k", { value: 7, writable: true, enumerable: true, configurable: true });
dp.m = 8;
console.log("dp", dp.k, dp.m, Object.keys(dp).join(","));

// accessors migrate
var acc = { base: 10 };
Object.defineProperty(acc, "twice", {
    get: function () { return this.base * 2; },
    enumerable: true,
    configurable: true,
});
acc.base = 21;
console.log("acc", acc.twice, Object.keys(acc).join(","));

// getOwnPropertyDescriptor on a shaped object
var god = { s: "str", n: 4.25 };
var d = Object.getOwnPropertyDescriptor(god, "n");
console.log("desc", d.value, d.writable, d.enumerable, d.configurable, d.get === undefined);

// index-looking keys migrate
var idx = { name: "x" };
idx["0"] = "zero";
idx.after = true;
console.log("idx", idx[0], idx.name, idx.after, Object.keys(idx).join(","));

// freeze/seal
var froz = { f: 1, g: 2 };
Object.freeze(froz);
froz.f = 99;
froz.h = 3;
console.log("froz", froz.f, froz.h, Object.isFrozen(froz), Object.isExtensible(froz));
var seal = { f: 1 };
Object.seal(seal);
seal.f = 2;
delete seal.f;
console.log("seal", seal.f, Object.isSealed(seal));

// preventExtensions keeps existing fields writable
var pe = { a: 1 };
Object.preventExtensions(pe);
pe.a = 2;
pe.b = 3;
console.log("pe", pe.a, pe.b, Object.isExtensible(pe));

// for-in order, proto chain
var proto = { inherited: "p" };
var child = Object.create(proto);
child.own1 = 1;
child.own2 = 2;
var forin = [];
for (var k in child) forin.push(k);
console.log("forin", forin.join(","));
console.log("hasOwn", child.hasOwnProperty("own1"), child.hasOwnProperty("inherited"), "inherited" in child);

// Object.assign shaped -> shaped and shaped -> dict
var tgt = { t: 0 };
var src = { u: 1, v: "two" };
Object.assign(tgt, src);
console.log("assign", JSON.stringify(tgt));
var dictTgt = { q: 1 };
delete dictTgt.q; // dict mode now
Object.assign(dictTgt, { r: 2, s: 3 });
console.log("assign2", JSON.stringify(dictTgt));

// defineProperties driven by a shaped descriptor object
var dst = {};
Object.defineProperties(dst, {
    one: { value: 1, enumerable: true, writable: true, configurable: true },
    two: { value: 2, enumerable: true },
});
console.log("defprops", dst.one, dst.two, Object.keys(dst).join(","));

// symbol keys migrate but stay invisible to string enumeration
var sym = Symbol("secret");
var symObj = { visible: 1 };
symObj[sym] = "hidden";
symObj.visible2 = 2;
console.log("sym", symObj[sym], Object.keys(symObj).join(","), Object.getOwnPropertySymbols(symObj).length);

// wide object crossing the slot-growth boundaries (4/8/16/32)
var wide = {};
for (var i = 0; i < 40; i++) wide["f" + i] = i;
var wsum = 0;
for (var i = 0; i < 40; i++) wsum += wide["f" + i];
console.log("wide", wsum, Object.keys(wide).length, wide.f0, wide.f39);

// long-lived churn: many transitions on one object graph
var churn = {};
for (var i = 0; i < 60; i++) {
    churn["k" + i] = i;
    if (i % 7 === 0) churn["k" + i] = "flip" + i;
}
console.log("churn", Object.keys(churn).length, churn.k0, churn.k7, churn.k59);

// JSON round-trip of shaped objects
var jr = JSON.parse('{"a":1,"b":[1,2,3],"c":{"d":"e"}}');
jr.f = jr.a + jr.b[2];
console.log("json", JSON.stringify(jr));

// spread/rest-free duplicate-literal shapes share transitions
function mk(x, y) { return { x: x, y: y }; }
var pts = [];
for (var i = 0; i < 100; i++) pts.push(mk(i, i * 2));
var psum = 0;
for (var i = 0; i < 100; i++) psum += pts[i].x + pts[i].y;
console.log("pts", psum);

// value update through Object.defineProperty on an existing shaped field
var upd = { z: 1 };
Object.defineProperty(upd, "z", { value: "replaced" });
console.log("upd", upd.z, Object.keys(upd).join(","));

// toString / propertyIsEnumerable / valueOf via proto on shaped receivers
var pie = { e: 1 };
console.log("pie", pie.propertyIsEnumerable("e"), pie.propertyIsEnumerable("nope"), Object.prototype.toString.call(pie));
