// generator: babel-node

var obj = { length: 3, 0: "hola", 1: "world", 2: 3.14 };

var arr = Array.from(obj);
console.log(arr.length);
console.log(arr.toString());

var arr2 = arr.slice(1);
console.log(arr2.length);
console.log(arr2.toString());

arr2[0] = 13.69;
console.log(arr2.toString());
