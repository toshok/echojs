var a = [1, 2, 3];
a = a.concat([4, 5, 6]);

console.log(a.length);
console.log(a[5]);

a = a.concat(7);

console.log(a.length);
console.log(a[6]);

/*
var a = null;
a = Array.prototype.concat.call(a,[4, 5, 6]);
console.log (a.length);
console.log (a[2]);
*/
