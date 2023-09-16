var key = undefined;
var obj = {};

console.log(obj[key]);

obj[key] = "hello world";

console.log(obj[key]);

obj[null] = "goodbye, world";
console.log(obj[null]);
console.log(obj[undefined]);
