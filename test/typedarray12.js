// generator: none

let array = Int16Array.of(2, 5, 9);
console.log(array.indexOf(2));     // 0
console.log(array.indexOf(7));     // -1
console.log(array.indexOf(9, 2));  // 2
console.log(array.indexOf(2, -1)); // -1
console.log(array.indexOf(2, -3)); // 0
console.log(array.indexOf()); // -1
console.log(array.indexOf("abc")); // -1

