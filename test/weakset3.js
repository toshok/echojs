// adapted from kangax's tests

var weakset = new WeakSet();
var obj = {};
console.log(weakset.add(obj) === weakset);
