// gc-plan P0: a collection triggered while EXECUTING ON the generator's
// malloc'd stack (generator bodies call _ejs_gc_alloc).  Before the P0 fix
// mark_thread_stack scanned [&local, main-stack-bottom) from the generator
// stack — a bogus range spanning unmapped memory: instant segfault under
// EJS_GC_EVERY_N_ALLOC=7, silent overscan otherwise.
function* g() {
    var keep = [];
    for (var i = 0; i < 4000; i++) {
        keep.push({ a: i, b: i + 1 });
        if (i % 1000 === 0) yield i;
    }
    var sum = 0;
    for (var j = 0; j < keep.length; j += 100) sum += keep[j].a;
    yield sum;
}
var it = g();
var r = it.next();
var out = [];
while (!r.done) { out.push(r.value); r = it.next(); }
console.log(out.join(","));
