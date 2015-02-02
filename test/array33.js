
console.log([1,2,3].indexOf(1));
console.log([1,2,3,1].indexOf(5));
console.log([].indexOf(5));


console.log([1,2,3].lastIndexOf(1));
console.log([1,2,3,1].lastIndexOf(1));
console.log([1,2,3].lastIndexOf(5));
console.log([].lastIndexOf(5));

var indexOf = Array.prototype.indexOf;
var lastIndexOf = Array.prototype.lastIndexOf;

var testArr1 = { length: 3, 0: 1, 1: 2, 2: 3 };
var testArr2 = { length: 4, 0: 1, 1: 2, 2: 3, 3: 1 };
var testArr3 = { length: 0 };
var testArr4 = { length: 4, 0: 1, 1: 2, 2: 3 }; // missing index 3

console.log(indexOf.call(testArr1, 1));
console.log(indexOf.call(testArr2, 5));
console.log(indexOf.call(testArr3, 5));
console.log(indexOf.call(testArr4, 3));

console.log(lastIndexOf.call(testArr1, 1));
console.log(lastIndexOf.call(testArr2, 5));
console.log(lastIndexOf.call(testArr3, 5));
console.log(lastIndexOf.call(testArr4, 1));
