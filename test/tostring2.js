console.log(Function.prototype.toString);
console.log(Function.prototype.toString == Object.prototype.toString);
console.log(Object.prototype.toString);
console.log(Object.prototype.toString.call(Object.prototype.toString));
console.log(Object.prototype.toString.call(5));
console.log(Object.prototype.toString.apply(Object.prototype.toString));
console.log(Object.prototype.toString.apply(5));

console.log(Object.prototype.toString.call([]));
console.log(Object.prototype.toString.apply([]));

console.log(Object.prototype.toString.call(/hi/));
