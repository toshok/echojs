var str = "hello, world";

console.log(str.replace("hello, world", "i said: $&"));

// the tail after the match is preserved, including a single-character one
console.log("abc".replace("b", "x"));
console.log("abcd".replace("bc", "x"));
console.log("abc".replace("c", "x"));
console.log("abc".replace("b", "[$`]"));
