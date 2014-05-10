var str = "To be, or not to be, that is the question.";

console.log(str.contains("To be"));       // true
console.log(str.contains("question"));    // true
console.log(str.contains("nonexistent")); // false
console.log(str.contains("To be", 1));    // false
console.log(str.contains("TO BE"));       // false
