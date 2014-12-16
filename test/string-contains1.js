var str = "To be, or not to be, that is the question.";

console.log(str.includes("To be"));       // true
console.log(str.includes("question"));    // true
console.log(str.includes("nonexistent")); // false
console.log(str.includes("To be", 1));    // false
console.log(str.includes("TO BE"));       // false
