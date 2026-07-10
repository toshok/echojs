/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

// The %-intrinsic calls EIR knows how to lower, keyed by callee name.
// Pre-EIR desugar passes (see preEIRConvert in closure-conversion.js)
// rewrite constructs lowering has no native form for into calls of these
// intrinsics, which both pipelines then understand: the legacy visitor
// through its ejs_intrinsics table, EIR through this one.
//
// An entry is either
//   { op: "<eir opcode>" }        lower to that op, operands = the
//                                 visited arguments
//   { runtime: "<rt fn name>" }   lower to call_runtime imms.name (the
//                                 runtime function must take plain ejsval
//                                 arguments and return an ejsval)
//
// scopes.js consults this table to reject unknown intrinsics EARLY (a
// late LowerNotSupported abandons the whole file's EIR set), so keep it
// the single source of truth: never lower an intrinsic in lower.js that
// isn't listed here.

export const eir_intrinsics = {
    "%arrayFromSpread": { op: "array_from_spread" },
};
