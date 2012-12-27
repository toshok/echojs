
// an initializer in the middle of other declarations
var foo, arr = [], bar;

foo = 5;
arr.push(foo);
bar = {};
arr.push(bar);

console.log (arr.length);
