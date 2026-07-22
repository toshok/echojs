// the Phase 3 arithmetic microbenchmark kernel: tight loop of adds/muls/
// divs/compares over module-local {number} locals — everything the
// oracle can type, nothing else.  Also serves as a probe.
function kernel(n) {
    var s = 0;
    var i = 0;
    while (i < n) {
        s = s + i * i - i / 2;
        i = i + 1;
    }
    return s;
}
var out = 0;
var r = 0;
while (r < 40) {
    out = out + kernel(1000000);
    r = r + 1;
}
console.log(out);
