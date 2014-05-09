console.log(String.fromCodePoint(42));        // "*"
console.log(String.fromCodePoint(65, 90));    // "AZ"
console.log(String.fromCodePoint(0x404));     // "\u0404"
console.log(String.fromCodePoint(0x2F804));  // "\uD87E\uDC04"
console.log(String.fromCodePoint(194564));   // "\uD87E\uDC04"
console.log(String.fromCodePoint(0x1D306, 0x61, 0x1D307)); // "\uD834\uDF06a\uD834\uDF07"

try {
  String.fromCodePoint('_')       // RangeError
}
catch (e) {
  console.log(e);
}
try {
  String.fromCodePoint(Infinity); // RangeError
}
catch (e) {
  console.log(e);
}
try {
  String.fromCodePoint(-1);       // RangeError
}
catch (e) {
  console.log(e);
}
try {
  String.fromCodePoint(3.14);     // RangeError
}
catch (e) {
  console.log(e);
}
try {
  String.fromCodePoint(3e-2);     // RangeError
}
catch (e) {
  console.log(e);
}
try {
  String.fromCodePoint(NaN);      // RangeError
}
catch (e) {
  console.log(e);
}
