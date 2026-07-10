/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// thrown by scope analysis / lowering when a construct is outside the
// supported subset; compile() reports it as a compile error.
//
// deliberately NOT an Error subclass: this shape predates the legacy
// pipeline's removal (subclassing Error miscompiled there) and is now
// simply the stable, structurally-testable form of the signal.

import type { SourceLocation } from "../estree";

export interface LowerNotSupportedError extends Error {
    eir_lower_not_supported: true;
    what: string;
}

export function LowerNotSupported(
    what: string,
    loc?: SourceLocation | null
): LowerNotSupportedError {
    const locstr = loc && loc.start ? ` at ${loc.start.line}:${loc.start.column}` : "";
    const e = new Error(`EIR lowering does not support ${what}${locstr}`) as LowerNotSupportedError;
    e.eir_lower_not_supported = true;
    e.what = what;
    return e;
}

export function isLowerNotSupported(e: unknown): e is LowerNotSupportedError {
    return (
        typeof e === "object" &&
        e !== null &&
        (e as { eir_lower_not_supported?: boolean }).eir_lower_not_supported === true
    );
}
