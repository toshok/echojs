console.log("[" + "  hi  ".trimStart() + "]");
console.log("[" + "  hi  ".trimEnd() + "]");
console.log("[" + "hi".trimStart() + "]");
console.log("[" + "hi".trimEnd() + "]");
console.log("[" + "   ".trimStart() + "]");
console.log("[" + "   ".trimEnd() + "]");
console.log("[" + "".trimStart() + "]");
console.log("[" + "".trimEnd() + "]");

// same whitespace set as trim: tabs, newlines, nbsp, line/paragraph separators
console.log("[" + "\t\n\r hi \t\n\r".trimStart() + "]");
console.log("[" + "\t\n\r hi \t\n\r".trimEnd() + "]");
console.log("[" + "   hi   ".trimStart() + "]");
console.log("[" + "   hi   ".trimEnd() + "]");

// interior whitespace is untouched
console.log("[" + "  a b  ".trimStart() + "]");
console.log("[" + "  a b  ".trimEnd() + "]");

console.log("".trimStart.length, "".trimEnd.length);
