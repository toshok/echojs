
var a = [1,2,3,4,5];
var b = a.splice(1);
console.log (b[0]);
console.log (a[0]);

console.log();
console.log(a.length);
a.splice(1, 0, 6);
console.log(a.length);
console.log (a[0]);
console.log (a[1]);

console.log();
a = [1,2,3,4,5];
a.splice(1);
a.splice(1, 0, 6);
console.log (a[0]);
console.log (a[1]);
console.log (a[2]);
