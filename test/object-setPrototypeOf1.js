
var myproto = {};
var obj = Object.create(null);

console.log(Object.setPrototypeOf(obj, myproto) === obj);
console.log(Object.setPrototypeOf(obj, myproto) === obj);
