// generator: none

function valuePredicate(value, index, arr) {
    return value % 13 == 0;
}

function indexPredicate(value, index, arr) {
    return index == 3;
}

console.log(Int16Array.of().find(valuePredicate));      // undefined
console.log(Int16Array.of().findIndex(valuePredicate)); // -1
console.log(Int16Array.of().find(indexPredicate));      // undefined
console.log(Int16Array.of().findIndex(indexPredicate)); // -1
console.log();

console.log(Int16Array.of(1, 7, 9).find(valuePredicate));           // undefined
console.log(Int16Array.of(1, 7, 9).findIndex(valuePredicate));      // -1
console.log(Int16Array.of(1, 13, 26).find(valuePredicate));         // 13
console.log(Int16Array.of(1, 13, 26).findIndex(valuePredicate));    // 1
console.log(Int16Array.of(1, 9, 11, 13).find(valuePredicate));      // 13
console.log(Int16Array.of(1, 9, 11, 13).findIndex(valuePredicate)); // 3
console.log();

console.log(Int16Array.of(1).find(indexPredicate));                 // undefined
console.log(Int16Array.of(1).findIndex(indexPredicate));            // -1
console.log(Int16Array.of(1, 5, 7).find(indexPredicate));           // undefined
console.log(Int16Array.of(1, 5, 7).findIndex(indexPredicate));      // -1
console.log(Int16Array.of(1, 5, 7, 13).find(indexPredicate));       // 13
console.log(Int16Array.of(1, 5, 7, 13).findIndex(indexPredicate));  // 3
console.log();

let obj = { requiredValue: 7 };

function findValue(value, index, arr) {
    return value === this.requiredValue;
}

console.log(Int16Array.of().find(findValue, obj));              // undefined
console.log(Int16Array.of().findIndex(findValue, obj));         // -1
console.log(Int16Array.of(0, 3).find(findValue, obj));          // undefined
console.log(Int16Array.of(0, 3).findIndex(findValue, obj));     // -1
console.log(Int16Array.of(0, 3, 7).find(findValue, obj));       // 7
console.log(Int16Array.of(0, 3, 7).findIndex(findValue, obj));  // 2

