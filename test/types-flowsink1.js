// flow-sensitive field writes + partial-escape probe
// materialization (docs/sinking-plan.md).  Every line must match node
// exactly, with and without --types, under EJS_SHAPES=off, gc-stress,
// and -fno-flow-sink.

function branches(c, x, y) { var o = { a: 0 }; if (c) o.a = x; else o.a = y; return o.a; }
console.log(branches(true, 1, 2));
console.log(branches(false, 1, 2));

function loopAcc(n) {
    var o = { sum: 0, count: 0 };
    for (var i = 0; i < n; i++) { o.sum = o.sum + i; o.count = o.count + 1; }
    return o.sum + ":" + o.count;
}
console.log(loopAcc(0));
console.log(loopAcc(10));

function readBeforeWrite(x) { var o = { a: 5 }; var r = o.a; o.a = x; return r + "," + o.a; }
console.log(readBeforeWrite(9));

// partial escape: the object materializes at the call; identity and
// mutation through the alias must behave exactly
var captured = null;
function capture(o) { captured = o; return o; }
function escapes(x) {
    var o = { a: 1, b: 2 };
    o.a = x;
    var r = capture(o);
    return (r === captured) + ":" + captured.a + ":" + captured.b;
}
console.log(escapes(42));
captured.a = 77;
console.log(captured.a);

// escape via return: two calls yield distinct objects
function mk(a, b) { var o = { x: 0, y: 0 }; o.x = a; o.y = b; return o; }
var m1 = mk(1, 2), m2 = mk(1, 2);
console.log(m1.x, m1.y, m1 === m2);

// a fresh object per iteration escapes each time
function loopEscape(n) {
    var out = [];
    for (var i = 0; i < n; i++) { var o = { v: 0 }; o.v = i; out.push(o); }
    var s = "";
    for (var j = 0; j < out.length; j++) s += (j ? "," : "") + out[j].v;
    return s + ":" + (out[0] === out[1]);
}
console.log(loopEscape(4));

// declined shapes keep exact semantics: key-adding write
function addsKey(x) { var o = { a: 1 }; o.b = x; return o.a + ":" + o.b; }
console.log(addsKey(3));

// write inside try
function tryWrite(x) { var o = { a: 1 }; try { o.a = x; } catch (e) { o.a = -1; } return o.a; }
console.log(tryWrite(8));

// self-reference declines
function selfRef() { var o = { a: null }; o.a = o; return o.a === o; }
console.log(selfRef());

// a setter installed on Object.prototype must intercept the (declined)
// key-adding write — the epoch-free soundness pin
Object.defineProperty(Object.prototype, "zz", {
    set: function (v) { this._zz = v * 2; },
    get: function () { return this._zz; },
    configurable: true,
});
function addsZZ(x) { var o = { a: 1 }; o.zz = x; return o.zz; }
console.log(addsZZ(21));
delete Object.prototype.zz;
