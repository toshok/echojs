// xfail: sparse array support is pretty week and full of NOT_IMPLEMENTED's
var arr = new Array(1000000000);
arr[0] = "Hello World";
console.log (arr[0]);
console.log(arr.length);
