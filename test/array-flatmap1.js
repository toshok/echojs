console.log([1, 2, 3].flatMap(function (x) { return [x, x * 2]; }));
console.log([1, 2, 3].flatMap(function (x) { return x * 2; }));
console.log([1, 2, 3].flatMap(function (x) { return x == 2 ? [] : [x]; }));

// only one level is flattened
console.log([1, 2].flatMap(function (x) { return [[x]]; }));

// callback gets (element, index, array)
console.log(["a", "b"].flatMap(function (el, ix, arr) { return [el, ix, arr.length]; }));

// thisArg
var ctx = { factor: 10 };
console.log([1, 2].flatMap(function (x) { return [x * this.factor]; }, ctx));

// holes are skipped
console.log([1, , 3].flatMap(function (x) { return [x]; }));

console.log([].flatMap(function (x) { return [x]; }));

try { [1].flatMap(); } catch (e) { console.log("no-callback:", e.constructor.name); }
try { [1].flatMap("nope"); } catch (e) { console.log("non-callable:", e.constructor.name); }

console.log([].flatMap.length);
