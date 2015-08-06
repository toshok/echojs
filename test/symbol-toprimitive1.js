// generator: none

var a = {};
a[Symbol.toPrimitive] = function() { return 7; };
console.log(a == 7);
