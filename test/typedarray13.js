// generator: none

let arr = Int32Array.of(1, 7, 99, 12, 7, 67, 1);

console.log(arr.lastIndexOf(1));    // 6
console.log(arr.lastIndexOf(7));    // 4
console.log(arr.lastIndexOf(99));   // 2
console.log(arr.lastIndexOf(12));   // 3
console.log();

console.log(arr.lastIndexOf(1, 1)); // 0
console.log(arr.lastIndexOf(1, 6)); // 6
console.log(arr.lastIndexOf(7, 0)); // -1
console.log(arr.lastIndexOf(7, 1)); // 1
console.log(arr.lastIndexOf(7, 6)); // 4
console.log(arr.lastIndexOf(7, 99)); // 4
console.log();

console.log(arr.lastIndexOf(1, -1)); // 6
console.log(arr.lastIndexOf(1, -3)); // 0
console.log(arr.lastIndexOf(7, -1)); // 4
console.log(arr.lastIndexOf(7, -3)); // 4
console.log(arr.lastIndexOf(7, -4)); // 1
console.log(arr.lastIndexOf(1, -99)); // -1
console.log(arr.lastIndexOf(7, -99)); // -1
console.log();

console.log(arr.lastIndexOf(-7));   // -1
console.log(arr.lastIndexOf());     // -1
console.log(arr.lastIndexOf("x"));  // -1
console.log(new Int32Array(0).lastIndexOf(0)); // -1

