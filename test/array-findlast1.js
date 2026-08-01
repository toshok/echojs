console.log([1, 2, 3, 4].findLast(function (x) { return x % 2 == 1; }));
console.log([1, 2, 3, 4].findLast(function (x) { return x % 2 == 0; }));
console.log([1, 2, 3].findLast(function (x) { return x > 10; }));
console.log([].findLast(function (x) { return true; }));

// iterates from the end: first hit wins
var visited = [];
console.log([1, 2, 3].findLast(function (x) { visited.push(x); return x < 3; }));
console.log(visited);

// callback gets (element, index, array)
console.log(["a", "b"].findLast(function (el, ix, arr) { return ix == 0 && arr.length == 2; }));

// thisArg
var ctx = { min: 2 };
console.log([1, 2, 3].findLast(function (x) { return x >= this.min; }, ctx));

// undefined elements can be found
console.log([1, undefined].findLast(function (x) { return x === undefined; }));

try { [1].findLast(); } catch (e) { console.log("no-predicate:", e.constructor.name); }

console.log([].findLast.length);
