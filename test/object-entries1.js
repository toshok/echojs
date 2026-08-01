console.log(Object.entries({ a: 1, b: "two", c: true }));
console.log(Object.entries({}));
console.log(Object.entries({ 5: "five", a: 1 }));
console.log(Object.entries("ab"));
console.log(Object.entries([10, 20]));

// only own enumerable string-keyed properties
var proto = { inherited: 1 };
var obj = Object.create(proto);
obj.own = 2;
Object.defineProperty(obj, "hidden", { value: 3, enumerable: false });
console.log(Object.entries(obj));

console.log(Object.entries.length);

try { Object.entries(null); } catch (e) { console.log("null:", e.constructor.name); }
try { Object.entries(undefined); } catch (e) { console.log("undefined:", e.constructor.name); }
