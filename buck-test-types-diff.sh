#!/bin/bash
# The Phase 3 --types diff lane (docs/maam-plan.md, P3 gate): compile every
# test/*.js twice with the node-hosted compiler — flag-off and --types — run
# both executables, and byte-compare RUN STDOUT.  Any divergence is a Phase 3
# stop-the-line bug: --types may only change code size/speed, never behavior.
#
# Protocol (lessons from the measurement chunks baked in):
#   - stdout only: --types adds stats lines to stderr by design, and the
#     debug runtime traces normally-handled EXCEPTIONS to stderr;
#   - color-free: node inherits FORCE_COLOR from dev shells (and a buck
#     daemon started from one) — everything runs under NO_COLOR with
#     FORCE_COLOR stripped;
#   - files that fail to compile flag-off are N/A (tester.js — an esprima
#     parse gap — is the standing one), not lane failures;
#   - per-file timeout discipline (120 s, kill and record);
#   - the maam CJS dist must be built (external-deps/echojs-maam:
#     `npm run build && npm run build:cjs`).
#
# Standalone by design (not genrule-wired: a ~15-minute double compile of
# the whole suite is a CI-lane decision, not a default build step).  Run
# from the repo root after `buck2 build //lib:generated //:srcdir-tree`:
#
#   ./buck-test-types-diff.sh <work-tree> <log-dir> [concurrency]
#
# where <work-tree> is a stage0-style tree (srcdir-tree + lib/generated +
# test/) — the caller assembles it so this script never mixes trees
# (franken-tree lesson).  Writes per-file logs + results.jsonl to
# <log-dir> and prints the summary table.
set -euo pipefail

# Absolutize both paths up front: a relative <work-tree> once resolved
# against each worker's cwd, turning every compile into an N/A and the
# lane into a 100%-N/A exit-0 "PASS" (review finding M1).  The tree must
# also live INSIDE the repo checkout so the probe can find
# external-deps/echojs-maam — from /tmp the oracle silently skips and the
# lane tests nothing Phase-3-specific (guarded below by diamonds==0).
WORK="$(cd "$1" && pwd)"
mkdir -p "$2"
LOGDIR="$(cd "$2" && pwd)"
CONC="${3:-4}"

export NODE_PATH="/Users/toshok/src/echojs/echojs/node_modules:/Users/toshok/src/echojs/echojs/node-llvm/build/Release"
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
if [ "$(uname -s)" = "Darwin" ]; then
    export SDKROOT="${SDKROOT:-$(/usr/bin/xcrun --show-sdk-path)}"
fi
export NO_COLOR=1
unset FORCE_COLOR

WORK="$WORK" LOGDIR="$LOGDIR" CONC="$CONC" exec node --input-type=module -e '
import { spawn } from "node:child_process";
import * as fs from "node:fs";
import * as path from "node:path";

const WORK = process.env.WORK;
const LOGDIR = process.env.LOGDIR;
const CONC = Number(process.env.CONC || 4);
const TIMEOUT_MS = 120000;
const testDir = path.join(WORK, "test");

const files = fs.readdirSync(testDir).filter((f) => f.endsWith(".js") && !f.includes("/")).sort();

function run(cmd, args, opts, timeoutMs) {
    return new Promise((resolve) => {
        const child = spawn(cmd, args, { ...opts, stdio: ["ignore", "pipe", "pipe"] });
        let out = Buffer.alloc(0), err = Buffer.alloc(0), timedout = false, failed = null;
        child.stdout.on("data", (d) => (out = Buffer.concat([out, d])));
        child.stderr.on("data", (d) => (err = Buffer.concat([err, d])));
        const t = setTimeout(() => { timedout = true; child.kill("SIGKILL"); }, timeoutMs);
        // a spawn failure (ENOENT — e.g. an exe that never materialized)
        // must be a recorded per-file anomaly, never a lane crash
        child.on("error", (e) => { clearTimeout(t); failed = String(e); resolve({ code: -1, out, err, timedout, failed }); });
        child.on("exit", (code) => { clearTimeout(t); resolve({ code, out, err, timedout, failed }); });
    });
}

const EJS = ["--srcdir", "--moduledir", "../node-compat", "--moduledir", "../ejs-llvm"];
const results = [];
let idx = 0;

async function worker(wid) {
    // per-worker TMPDIR: concurrent compiles never share temp space
    const tmp = path.join(LOGDIR, "tmp" + wid);
    fs.mkdirSync(tmp, { recursive: true });
    const env = { ...process.env, TMPDIR: tmp };
    for (;;) {
        const file = files[idx++];
        if (!file) return;
        const base = file.replace(/\.js$/, "");
        const exe = path.join(testDir, file + ".exe");
        const r = { file, status: "?", diamonds: 0, queries: 0, unknown: 0 };

        // flag-off compile + run
        const c0 = await run("node", [path.join(WORK, "lib/generated/ejs-es6.js"), ...EJS, file], { cwd: testDir, env }, TIMEOUT_MS);
        if (c0.timedout) { r.status = "TIMEOUT-compile-off"; results.push(r); continue; }
        if (c0.code !== 0) { r.status = "N/A"; results.push(r); continue; }
        const off = await run(exe, [], { cwd: testDir, env }, TIMEOUT_MS);
        if (off.failed) { r.status = "RUN-OFF-SPAWN-FAIL"; results.push(r); continue; }
        if (off.timedout) { r.status = "TIMEOUT-run-off"; results.push(r); continue; }

        // --types compile + run
        const c1 = await run("node", [path.join(WORK, "lib/generated/ejs-es6.js"), ...EJS, "--types", file], { cwd: testDir, env }, TIMEOUT_MS);
        if (c1.timedout) { r.status = "TIMEOUT-compile-on"; results.push(r); continue; }
        if (c1.code !== 0) { r.status = "TYPES-COMPILE-FAIL"; results.push(r); continue; }
        fs.writeFileSync(path.join(LOGDIR, base + ".types.err"), c1.err);
        const m = String(c1.err).match(/diamonds=(\d+) oracleQueries=(\d+) oracleUnknown=(\d+)/g) || [];
        for (const line of m) {
            const g = line.match(/diamonds=(\d+) oracleQueries=(\d+) oracleUnknown=(\d+)/);
            r.diamonds += +g[1]; r.queries += +g[2]; r.unknown += +g[3];
        }
        const on = await run(exe, [], { cwd: testDir, env }, TIMEOUT_MS);
        if (on.failed) { r.status = "RUN-ON-SPAWN-FAIL"; results.push(r); continue; }
        if (on.timedout) { r.status = "TIMEOUT-run-on"; results.push(r); continue; }

        if (Buffer.compare(off.out, on.out) === 0 && off.code === on.code) {
            r.status = "IDENTICAL";
        } else {
            r.status = "DIVERGENT";
            fs.writeFileSync(path.join(LOGDIR, base + ".off.out"), off.out);
            fs.writeFileSync(path.join(LOGDIR, base + ".on.out"), on.out);
        }
        results.push(r);
        process.stdout.write(`${file} ${r.status} diamonds=${r.diamonds}\n`);
    }
}

await Promise.all(Array.from({ length: CONC }, (_, i) => worker(i)));

fs.writeFileSync(path.join(LOGDIR, "results.jsonl"), results.map((r) => JSON.stringify(r)).join("\n") + "\n");
const by = (s) => results.filter((r) => r.status === s);
const identical = by("IDENTICAL"), divergent = by("DIVERGENT"), na = by("N/A");
const other = results.filter((r) => !["IDENTICAL", "DIVERGENT", "N/A"].includes(r.status));
const tot = (k) => results.reduce((a, r) => a + r[k], 0);
console.log("==== --types diff lane summary ====");
console.log(`files: ${results.length}  identical: ${identical.length}  divergent: ${divergent.length}  N/A: ${na.length}  other: ${other.length}`);
console.log(`diamonds total: ${tot("diamonds")}  oracleQueries: ${tot("queries")}  oracleUnknown: ${tot("unknown")}`);
if (divergent.length) { console.log("DIVERGENT:", divergent.map((r) => r.file).join(" ")); process.exit(1); }
if (other.length) { console.log("OTHER:", other.map((r) => `${r.file}:${r.status}`).join(" ")); process.exit(1); }
// Vacuous-pass guards (review findings M1/M2): a lane that compared zero
// files, or ran with no live oracle (no diamonds anywhere), proves nothing.
if (identical.length === 0) { console.log("LANE FAIL: zero files compared (all N/A) — bad work tree?"); process.exit(1); }
if (tot("diamonds") === 0) { console.log("LANE FAIL: diamonds total is 0 — no live oracle (work tree outside the repo checkout, or maam dist unbuilt); the lane tested nothing Phase-3-specific"); process.exit(1); }
console.log("LANE PASS: zero divergence");
'
