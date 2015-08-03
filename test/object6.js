// xfail: we don't define everything we should.

var a = {b:10};
function c() {}

Object.getOwnPropertyNames(a).forEach(function (key) { console.log (key); });
console.log("---");
Object.getOwnPropertyNames(c).forEach(function (key) { console.log (key); });
console.log("---");
Object.getOwnPropertyNames(c.prototype).forEach(function (key) { console.log (key); });
