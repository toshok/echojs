// generator: none

// Array.prototype.slice from kangax

class C extends Array {}
var c = new C();
c.push(2,4,6);
console.log(c.slice(1,2) instanceof C);
