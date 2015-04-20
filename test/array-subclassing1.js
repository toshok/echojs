// basic functionality from kangax
class C extends Array {}
var c = new C();
var len1 = c.length;
c[2] = 'foo';
var len2 = c.length;
c.length = 1;
console.log(len1 === 0 && len2 === 3 && c.length === 1 && !(2 in c));
