// generator: none

var a = [], b = [], c = [1,2,3];
b[Symbol.isConcatSpreadable] = false;
a = a.concat(b);
console.log (a[0] === b);

a = a.concat(c);
console.log(a.length === 4);
