
var a=[], i = 0;
a[i++] += 5;
console.log(i);

var a = [];
var b = {};
i = 0;
var j = 0;
Object.defineProperty(b, "foo", { get: function() { j++; return a; } } );
b.foo[i++] += 5;
console.log(i);
console.log(j);
