// GC triggers while running ON the generator's stack
function* g() {
    var keep = [];
    for (var i = 0; i < 200000; i++) {
        keep.push({ a: i, b: i + 1 });
        if (i % 50000 === 0) yield i;
    }
    var sum = 0;
    for (var j = 0; j < keep.length; j += 10000) sum += keep[j].a;
    yield sum;
}
var it = g();
var r = it.next();
var out = [];
while (!r.done) { out.push(r.value); r = it.next(); }
console.log(out.join(","));
