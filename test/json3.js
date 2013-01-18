if (typeof console !== "undefined") print = console.log;
var a = { b: 1, c: 2, d: 3, e: 4, f: 5 };
var a_str = JSON.stringify (a);
var b = JSON.parse(a_str);
console.log (b.b);
console.log (b.c);
console.log (b.d);
console.log (b.e);
console.log (b.f);
