// Map.prototype.delete: was an unimplemented runtime stub (returned
// false, removed nothing) until the optimizer's slot-load CSE became
// its first compiler-side caller.  Pins removal, size, has, get,
// iteration skipping, the return value, and re-adding after delete.

var m = new Map();
m.set("a", 1);
m.set("b", 2);
m.set("c", 3);

console.log(m.delete("b"));
console.log(m.delete("nope"));
console.log(m.size);
console.log(m.has("b"));
console.log(m.get("b"));

var keys = [];
m.forEach(function (v, k) {
    keys.push(k + "=" + v);
});
console.log(keys.join(","));

m.set("b", 9);
console.log(m.size);
console.log(m.get("b"));

var it = m.keys();
var r;
var order = [];
while (!(r = it.next()).done) order.push(r.value);
console.log(order.join(","));
