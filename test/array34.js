var arr = [];
console.log(arr.sort().join());

arr.push("a");
console.log(arr.sort().join());

arr.push("b");
console.log(arr.sort().join());

arr.push(1);
console.log(arr.sort().join());

arr.push(123);
console.log(arr.sort().join());

arr.push("b");
arr.push("a");
console.log(arr.sort().join());

arr.push(2.1718);
arr.push(3.1416);
arr.push(13);
arr.push(69);
arr.push("ahoj");
arr.push("hello");
arr.push("hola");
arr.push("bonsoir");

console.log(arr.sort().join());

// again, just in case, to make sure we kept the sorting.
console.log(arr.sort().join());
