console.log(Object.values({ a: 1, b: "two", c: true }));
console.log(Object.values({}));
console.log(Object.values({ 5: "five", a: 1 }));
console.log(Object.values("ab"));
console.log(Object.values([10, 20]));

// only own enumerable string-keyed properties
var proto = { inherited: 1 };
var obj = Object.create(proto);
obj.own = 2;
console.log(Object.values(obj));

console.log(Object.values.length);
console.log(Object.values.name);
