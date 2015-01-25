
function isGreaterThanTen(value, index, arr) {
    return value > 10;
}

console.log(Int16Array.of().every(isGreaterThanTen));               // true
console.log(Int16Array.of(1, 3, 7).every(isGreaterThanTen));        // false
console.log(Int16Array.of(9, 10, 11).every(isGreaterThanTen));      // false
console.log(Int16Array.of(11, 11, 13).every(isGreaterThanTen));     // true
console.log();

function isGreaterThanValue(value, index, arr) {
    return value > this.expectedValue;
}

let obj = { expectedValue: 7 };

console.log(Int16Array.of().every(isGreaterThanValue, obj));            // true
console.log(Int16Array.of(1, 3, 7).every(isGreaterThanValue, obj));     // false
console.log(Int16Array.of(7).every(isGreaterThanValue, obj));           // false
console.log(Int16Array.of(8).every(isGreaterThanValue, obj));           // true
console.log(Int16Array.of(11, 13, 17).every(isGreaterThanValue, obj));  // true
console.log(Int16Array.of(128, 133, -1).every(isGreaterThanValue, obj));// false

