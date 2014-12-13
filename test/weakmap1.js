// adapted from kangax's tests

var key = {};
var weakmap = new WeakMap();

weakmap.set(key, 123);

console.log(weakmap.has(key) && weakmap.get(key) === 123);
