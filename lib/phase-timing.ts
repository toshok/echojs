/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// Per-phase wall accounting for the compile pipeline, enabled by
// EJS_COMPILE_PHASES=1 in the environment.  The env var is read once at
// module init (process.env is a rebuild-the-whole-environment getter
// under the self-hosted runtime — the pass-config rule).  Disabled, the
// wrappers are a function call and a branch; no Date.now() is taken.
//
// Buckets are wall-clock and inclusive: "verify" runs inside
// "lower+opt", so the table's lines don't sum to the total.  The report
// marks nested buckets.

const enabled =
    typeof process !== "undefined" && !!(process.env && process.env["EJS_COMPILE_PHASES"]);

interface PhaseTotal {
    ms: number;
    count: number;
}

const totals = new Map<string, PhaseTotal>();
// report in first-recorded order — pipeline order, since phase 1 of
// module 1 records first
const order: string[] = [];

export function phasesEnabled(): boolean {
    return enabled;
}

function accumulate(name: string, ms: number): void {
    let t = totals.get(name);
    if (!t) {
        t = { ms: 0, count: 0 };
        totals.set(name, t);
        order.push(name);
    }
    t.ms += ms;
    t.count += 1;
}

export function timePhase<T>(name: string, f: () => T): T {
    if (!enabled) return f();
    const started = Date.now();
    try {
        return f();
    } finally {
        accumulate(name, Date.now() - started);
    }
}

// bracket form of timePhase, for spans that don't wrap cleanly in a
// closure.  phaseStart() returns -1 when disabled; phaseEnd ignores it.
export function phaseStart(): number {
    return enabled ? Date.now() : -1;
}

export function phaseEnd(name: string, started: number): void {
    if (started < 0) return;
    accumulate(name, Date.now() - started);
}

// nested buckets (counted inside another phase's time) get a marker so
// the table reads correctly
const NESTED = new Set(["verify"]);

export function reportPhases(write: (line: string) => void): void {
    if (!enabled || order.length === 0) return;
    write("compile phases (wall ms):");
    let total = 0;
    for (const name of order) {
        const t = totals.get(name)!;
        const nested = NESTED.has(name);
        if (!nested) total += t.ms;
        write(
            `  ${name.padEnd(14)} ${String(t.ms).padStart(8)}  (${t.count}x)` +
                (nested ? "  [inside lower+opt]" : "")
        );
    }
    write(`  ${"total".padEnd(14)} ${String(total).padStart(8)}`);
}
