function f(n) {
    var o = { a: n, b: n + 1 };
    return o.a + o.b;
}
var s = 0; var i = 0;
while (i < 100) { s = s + f(i); i = i + 1; }
console.log(s);
