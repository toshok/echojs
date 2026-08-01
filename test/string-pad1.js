console.log("5".padStart(3, "0"));
console.log("5".padEnd(3, "0"));
console.log("abc".padStart(6));
console.log("abc".padEnd(6) + "|");

// maxLength <= length returns the string unchanged
console.log("abcdef".padStart(3, "0"));
console.log("abcdef".padEnd(6, "0"));
console.log("abc".padStart(-1));

// multi-char fill repeats and truncates
console.log("x".padStart(7, "abc"));
console.log("x".padEnd(7, "abc"));
console.log("x".padStart(4, "abc"));

// empty fill returns the string unchanged
console.log("x".padStart(5, ""));

// fillString goes through ToString
console.log("x".padStart(4, 0));

// maxLength goes through ToLength
console.log("x".padStart("4", "y"));
console.log("x".padStart(NaN, "y"));

console.log("".padStart(3, "ab"));

console.log("".padStart.length, "".padEnd.length);
