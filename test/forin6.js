var hasOwn = Object.prototype.hasOwnProperty;

var o = { a: 0, b: 1, c: 2, d: 3, e: 4, f: 5, g: 6, h: 7, i: 8 };

var p = {};

for (var k in o) {
    if (hasOwn.apply(o, [k])) {
        p[k] = o[k];
    }
}

console.log(p.a);
console.log(p.b);
console.log(p.c);
console.log(p.d);
console.log(p.e);
console.log(p.f);
console.log(p.g);
console.log(p.h);
console.log(p.i);
