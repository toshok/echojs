
var arr = [1,2,3];
arr.toString = Object.prototype.toString;
console.log (arr.toString());
