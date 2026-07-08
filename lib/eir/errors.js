/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

// thrown by scope analysis / lowering when a construct is outside the
// currently-supported subset; callers catch it and fall back to the legacy
// code path for that function.
//
// deliberately NOT a class: constructing an imported subclass of Error
// trips an IsConstructor assert when the compiler itself is compiled by
// the legacy pipeline (a latent legacy bug, still to be tracked down), so
// the fallback signal is a plain Error with a marker property, tested via
// isLowerNotSupported().

export function LowerNotSupported(what, loc) {
    let locstr = loc && loc.start ? ` at ${loc.start.line}:${loc.start.column}` : "";
    let e = new Error(`EIR lowering does not support ${what}${locstr}`);
    e.eir_lower_not_supported = true;
    e.what = what;
    return e;
}

export function isLowerNotSupported(e) {
    return e && e.eir_lower_not_supported === true;
}
