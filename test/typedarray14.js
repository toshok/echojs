
function valuePredicate(value, index, arr) {
    return value % 13 == 0;
}

function indexPredicate(value, index, arr) {
    return index == 3;
}

console.log(Int16Array.of().find(valuePredicate)); // -1
console.log(Int16Array.of().find(indexPredicate)); // -1
console.log();

console.log(Int16Array.of(1, 7, 9).find(valuePredicate));       // -1
console.log(Int16Array.of(1, 13, 26).find(valuePredicate));     // 1
console.log(Int16Array.of(1, 9, 11, 13).find(valuePredicate));  // 3
console.log();

console.log(Int16Array.of(1).find(indexPredicate));         // -1
console.log(Int16Array.of(1, 5, 7).find(indexPredicate));   // -1
console.log(Int16Array.of(1, 5, 7, 13).find(indexPredicate)); // 3
console.log();

let obj = { requiredValue: 7 };

function findValue(value, index, arr) {
    return value === this.requiredValue;
}

console.log(Int16Array.of().find(findValue, obj));      // -1
console.log(Int16Array.of(0, 3).find(findValue, obj));  // -1
console.log(Int16Array.of(0, 3, 7).find(findValue, obj)); // 2

