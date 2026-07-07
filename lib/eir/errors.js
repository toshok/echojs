/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

// thrown by scope analysis / lowering when a construct is outside the
// currently-supported subset; callers catch it and fall back to the legacy
// code path for that function.
export class LowerNotSupported extends Error {
    constructor(what, loc) {
        let locstr = loc && loc.start ? ` at ${loc.start.line}:${loc.start.column}` : "";
        super(`EIR lowering does not support ${what}${locstr}`);
        this.what = what;
    }
}
