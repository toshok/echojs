// adapted from kangax's tests

var weakmap = new WeakMap();
var key = {};
console.log(weakmap.set(key, 0) === weakmap);
