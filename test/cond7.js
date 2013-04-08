
var obj1 = { foo: "hi" };
var obj2 = null;

console.log (obj1 && obj1.foo || "bye");
console.log (obj2 && obj2.foo || "bye");
