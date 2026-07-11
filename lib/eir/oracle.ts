/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// Phase 0 of the MAAM type-oracle integration (docs/maam-plan.md): a
// plumbing probe.  With --types on, compile() hands us the desugared
// toplevel BEFORE collectEIRToplevel consumes it; we wrap its body as a
// Program (preserving node identity — the eventual oracle is keyed on
// the exact node objects), run the echojs-maam abstract interpreter
// over it, and log its stats.  Nothing downstream reads the result yet.
//
// Two hard rules, both load-bearing:
//   - maam is require()d lazily, only when the probe actually runs, so
//     a flag-off compile never touches it (and never pays for it);
//   - every failure here — missing/unbuilt submodule, an analysis
//     error, a self-hosted compiler with no host require() — degrades
//     to a compiler warning.  --types must never turn a compiling
//     program into a failing one.

import * as path from "@node-compat/path";
import * as fs from "@node-compat/fs";
import type * as e from "../estree";
import { reportWarning } from "../errors";

// The slice of maam's AnalysisResult the probe consumes, typed
// structurally so we never import (or resolve types from) the
// submodule itself.
interface MaamMetrics {
    reachedStates: number;
    configs: number;
    iterations: number;
    shapesInterned: number;
    unknownCalls: number;
    // added alongside this integration; absent in older builds
    degradedBindings?: number;
}

interface MaamResult {
    metrics: MaamMetrics;
    describe(): string;
    warnings(): Array<{ kind: string }>;
}

interface MaamModule {
    analyze(program: unknown, spec: unknown): MaamResult;
    kCFA(...args: unknown[]): unknown;
}

const MAAM_DIST_REL = ["external-deps", "echojs-maam", "dist", "cjs", "index.js"];
const MAAM_SUBMODULE_REL = ["external-deps", "echojs-maam"];

// undefined = not attempted yet; null = attempted and unavailable (the
// warning has already been issued — don't repeat it per module)
let cached_maam: MaamModule | null | undefined;

function errorMessage(err: unknown): string {
    if (err instanceof Error) return `${err.name}: ${err.message}`;
    return String(err);
}

// Locate and require() the maam CJS build.  We walk up from this
// module's directory looking for external-deps/echojs-maam — that works
// both for the source tree (lib/eir/) and for the babel'd node tree
// (lib/generated/lib/eir/, whose ancestors include the repo root).  A
// self-hosted (stage1+) compiler has no host require()/__dirname; the
// probe is a documented no-op-with-a-warning there.
function loadMaam(source_filename: string): MaamModule | null {
    if (cached_maam !== undefined) return cached_maam;
    cached_maam = null;

    if (typeof require !== "function" || typeof __dirname !== "string") {
        reportWarning(
            "--types is not available in a self-hosted compiler (no host require()); type analysis skipped.",
            source_filename
        );
        return null;
    }

    let submodule_dir: string | null = null;
    for (let dir = __dirname, prev = ""; dir !== prev; prev = dir, dir = path.dirname(dir)) {
        const sub = path.join(dir, ...MAAM_SUBMODULE_REL);
        if (!fs.existsSync(sub)) continue;
        submodule_dir = sub;
        const dist = path.join(dir, ...MAAM_DIST_REL);
        if (!fs.existsSync(dist)) break; // submodule present, build output missing
        try {
            cached_maam = require(dist) as MaamModule;
            return cached_maam;
        } catch (err) {
            reportWarning(
                `--types: failed to load echojs-maam from ${dist} (${errorMessage(err)}); type analysis skipped.`,
                source_filename
            );
            return null;
        }
    }

    reportWarning(
        submodule_dir !== null
            ? `--types: echojs-maam is present at ${submodule_dir} but its CJS build is missing; ` +
              "run `npm run build && npm run build:cjs` there. Type analysis skipped."
            : "--types: could not locate the external-deps/echojs-maam submodule; type analysis skipped.",
        source_filename
    );
    return null;
}

function warningSummary(warnings: Array<{ kind: string }>): string {
    if (warnings.length === 0) return "none";
    const counts = new Map<string, number>();
    for (const w of warnings) counts.set(w.kind, (counts.get(w.kind) || 0) + 1);
    return [...counts.entries()].map(([kind, n]) => `${kind}:${n}`).join(",");
}

// Run the probe over the module's desugared tree.  `tree` is the
// post-pre_eir_convert Program whose body[0] is the synthetic toplevel
// FunctionDeclaration (insert_toplevel_func) holding the module's
// statements.
export function runTypeAnalysisProbe(tree: e.Program, source_filename: string): void {
    const maam = loadMaam(source_filename);
    if (!maam) return;

    const toplevel = tree.body[0];
    if (!toplevel || toplevel.type !== "FunctionDeclaration") {
        reportWarning(
            "--types: expected the synthetic toplevel FunctionDeclaration; type analysis skipped.",
            source_filename
        );
        return;
    }

    // Same body array, same node objects — no cloning.
    const program = { type: "Program", sourceType: "script", body: toplevel.body.body };

    const started = Date.now();
    try {
        const result = maam.analyze(
            program,
            maam.kCFA(1, "flow-sensitive", "call-site", /*shapeCap*/ 64, false, false, false, /*stateCap*/ 512)
        );
        const wall = Date.now() - started;
        const m = result.metrics;
        console.warn(
            `--types: ${source_filename}: wall=${wall}ms reachedStates=${m.reachedStates} ` +
                `configs=${m.configs} iterations=${m.iterations} shapesInterned=${m.shapesInterned} ` +
                `unknownCalls=${m.unknownCalls} degradedBindings=${m.degradedBindings ?? 0} ` +
                `warnings=${warningSummary(result.warnings())}`
        );
        console.warn(result.describe());
    } catch (err) {
        const wall = Date.now() - started;
        reportWarning(
            `--types: analysis failed after ${wall}ms (${errorMessage(err)}); continuing without type information.`,
            source_filename
        );
    }
}
