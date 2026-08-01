console.log(Object.fromEntries([["a", 1], ["b", 2]]));
console.log(Object.fromEntries([]));

// later entries win
console.log(Object.fromEntries([["k", 1], ["k", 2]]));

// non-string keys go through ToPropertyKey
console.log(Object.fromEntries([[0, "zero"], [true, "yes"]]));

// any iterable of [key, value] pairs works
console.log(Object.fromEntries(new Map([["x", 10], ["y", 20]])));

// round trip through Object.entries
console.log(Object.fromEntries(Object.entries({ p: 1, q: 2 })));

console.log(Object.fromEntries.length);

try { Object.fromEntries(); } catch (e) { console.log("no-arg:", e.constructor.name); }
try { Object.fromEntries(null); } catch (e) { console.log("null:", e.constructor.name); }
try { Object.fromEntries([1]); } catch (e) { console.log("non-object entry:", e.constructor.name); }
