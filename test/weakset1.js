// generator: babel-node

// adapted from kangax's tests

var obj1 = {},
    obj2 = {};
var weakset = new WeakSet();

weakset.add(obj1);
weakset.add(obj1);

console.log(weakset.has(obj1));
