// regex flags must reach the matcher: ignoreCase and multiline were
// parsed into the RegExp object but never passed to PCRE

console.log("aAa".replace(/a/gi, "x"));
console.log(/HeLLo/i.test("hello"));
console.log("a\nb".replace(/^b/m, "B"));
console.log("AbC".match(/[a-z]+/i)[0]);
console.log(/x/i.flags ? /x/gi.ignoreCase : "no-flags");
