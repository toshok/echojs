// runtime-P2 microbenchmark: the types-bench1 kernel, EXPORTED.  The
// export pins the trusted path (the closure escapes through the
// non-promoted slot), so before runtime-P2 every cross-module call ran
// the fully generic body; the boundary wrapper recovers the typed
// kernel behind two per-call has_tag checks.
export function kernel(n) {
    var s = 0;
    var i = 0;
    while (i < n) {
        s = s + i * i - i / 2;
        i = i + 1;
    }
    return s;
}
// module-local numeric profile for the oracle
console.log(kernel(100));
