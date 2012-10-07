
var a = {foo:"hi"};
var b = {a:a};
var c = {b:b};
var d = {c:function() { return c; }}
console.log(0);
console.log (d.c().b.a.foo);
