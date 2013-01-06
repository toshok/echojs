
var a = {};

var props = {
  bar: { value: 5, configurable: true, writable: false, enumerable: false }
};

Object.defineProperty(props, "foo", { value: 10, configurable: true, writable: false, enumerable: false });

Object.defineProperties (a, props);

console.log (a.foo);
console.log (a.bar);