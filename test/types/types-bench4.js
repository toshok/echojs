// the sinking-P3 flow-sink microbenchmark: a loop-accumulator OBJECT
// whose fields are read and written every iteration.  With
// flow-sensitive sinking the object scalar-replaces into loop-carried
// values (allocation-free, memory-op-free); without it every iteration
// pays the read/write diamonds against a real heap object.  A/B:
// EJS_NO_FLOW_SINK=1 at compile time.
function accum(n) {
    var o = { sum: 0, weighted: 0, count: 0 };
    var i = 0;
    while (i < n) {
        o.sum = o.sum + i;
        o.weighted = o.weighted + i * 0.5;
        o.count = o.count + 1;
        i = i + 1;
    }
    return o.sum + o.weighted + o.count;
}
// the partial-escape twin: the accumulator escapes at the end of every
// call — materialization keeps the loop allocation-free and pays one
// allocation per call
var last = null;
function keep(o) { last = o; }
function accumEscape(n) {
    var o = { sum: 0, count: 0 };
    var i = 0;
    while (i < n) {
        o.sum = o.sum + i;
        o.count = o.count + 1;
        i = i + 1;
    }
    keep(o);
    return 1;
}
var out = 0;
var r = 0;
while (r < 20) {
    out = out + accum(1000000);
    out = out + accumEscape(1000000);
    r = r + 1;
}
console.log(out, last.sum, last.count);
