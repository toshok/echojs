
var a = {};

Object.defineProperties (a, {
			   foo: { value: 10, configurable: true, writable: false, enumerable: false },
			   bar: { value: 5, configurable: true, writable: false, enumerable: false }
			 });

console.log (a.foo);
console.log (a.bar);