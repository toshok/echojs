//generator: babel-node

try {
    console.log("abc".repeat(-1)); // RangeError
} catch (e) {
    console.log(e.constructor.name);
}

console.log("abc".repeat(0)); // ""
console.log("abc".repeat(1)); // "abc"
console.log("abc".repeat(2)); // "abcabc"
console.log("abc".repeat(3.5)); // "abcabcabc" (count will be converted to integer)
try {
    console.log("abc".repeat(1 / 0)); // RangeError
} catch (e) {
    console.log(e.constructor.name);
}

console.log({ toString: () => "abc", repeat: String.prototype.repeat }.repeat(2)); // "abcabc" (repeat is a generic method)
