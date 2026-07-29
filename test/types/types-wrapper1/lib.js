// runtime-P2 probe: the export-boundary wrapper.  kernel is EXPORTED —
// its closure escapes through the non-promoted module slot, so it is
// never TRUSTED-specialized (external callers are outside the
// analysis, and maam's constant-propagation claims don't survive
// them) — but it still gets the boundary wrapper: a has_tag(number)
// guard per formal at the generic entry, dispatching to an UNTRUSTED
// f64 clone whose guarded body folds structurally from the entry
// boxes.  specWrapped=1 in the stats line; behavior is identical to
// flag-off for every caller.
export function kernel(n) {
    var s = 0;
    var i = 0;
    while (i < n) {
        s = s + i * i - i / 2;
        i = i + 1;
    }
    return s;
}
// a module-local call gives the oracle its numeric profile; it reaches
// the same wrapper guards through the generic entry
console.log(kernel(10));
