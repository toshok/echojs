// generator: babel-node

// adapted from kangax's tests

var obj1 = {},
    obj2 = {};
var weakset = new WeakSet([obj1, obj2]);

console.log(weakset.has(obj1) && weakset.has(obj2));
