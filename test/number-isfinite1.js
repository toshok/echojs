console.log(Number.isFinite(Infinity)); // false
console.log(Number.isFinite(NaN)); // false
console.log(Number.isFinite(-Infinity)); // false

console.log(Number.isFinite(0)); // true
console.log(Number.isFinite(2e64)); // true

console.log(Number.isFinite("0")); // false, would've been true with global isFinite("0")
