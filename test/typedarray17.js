// generator: none

let sum = (prevValue, currValue) => prevValue + currValue;
let concat = (prevValue, currValue) => "" + prevValue + currValue;
let concatWithIndex = (prevValue, currValue, index) => "" + prevValue + index + currValue;

console.log(Int16Array.of().reduce(sum, 13)); // 13
console.log(Int16Array.of(1, 3, 5, 7).reduce(sum)); // 16
console.log(Int16Array.of(1, 3, 5, 7).reduce(sum, 13)); // 29
console.log(Int16Array.of(1, 3, 5, 7).reduce(concat)); // 1357
console.log(Int16Array.of(1, 3, 5, 7).reduce(concat, 13)); // 131357
console.log(Int16Array.of(1, 3, 5, 7).reduce(concatWithIndex)); // 1132537
console.log(Int16Array.of(1, 3, 5, 7).reduce(concatWithIndex, ":")); // :01132537

console.log(Int16Array.of().reduceRight(sum, 13)); // 13
console.log(Int16Array.of(1, 3, 5, 7).reduceRight(sum)); // 16
console.log(Int16Array.of(1, 3, 5, 7).reduceRight(sum, 13)); // 29
console.log(Int16Array.of(1, 3, 5, 7).reduceRight(concat)); // 7531
console.log(Int16Array.of(1, 3, 5, 7).reduceRight(concat, 13)); // 137531
console.log(Int16Array.of(1, 3, 5, 7).reduceRight(concatWithIndex)); // 7251301
console.log(Int16Array.of(1, 3, 5, 7).reduceRight(concatWithIndex, ":")); // :37251301
