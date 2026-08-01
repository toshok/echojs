console.log(Object.hasOwn({ a: 1 }, "a"));
console.log(Object.hasOwn({ a: 1 }, "b"));
console.log(Object.hasOwn({ a: undefined }, "a"));

// inherited properties are not own
var proto = { inherited: 1 };
var obj = Object.create(proto);
obj.own = 2;
console.log(Object.hasOwn(obj, "own"));
console.log(Object.hasOwn(obj, "inherited"));
console.log("inherited" in obj);

// non-enumerable own properties still count
var hidden = {};
Object.defineProperty(hidden, "h", { value: 1, enumerable: false });
console.log(Object.hasOwn(hidden, "h"));

// index keys go through ToPropertyKey
console.log(Object.hasOwn([10, 20], 0));
console.log(Object.hasOwn([10, 20], 2));
console.log(Object.hasOwn("ab", 1));
console.log(Object.hasOwn({ 3: true }, "3"));

console.log(Object.hasOwn.length);

try { Object.hasOwn(null, "a"); } catch (e) { console.log("null:", e.constructor.name); }
