#!/usr/bin/env node
// test262 subset probe.
//
// Host tooling, runs under node — this is not compiler input.  Compiles
// each selected test262 test with a built ejs (srcdir layout) and runs
// the binary, classifying the outcome.  See README.md here for the
// selection policy and the probe's simplifications.
//
//   node test/test262/run-test262.mjs run \
//     --suite  <test262 checkout> \
//     --ejs    <workroot with ./ejs + srcdir layout> \
//     [--jobs N] [--cap-builtins 3|all] [--stride-language 1] [--filter substr] \
//     [--shard K/N] \
//     [--out results.jsonl] [--expectations file [--update-expectations]]
//
//   node test/test262/run-test262.mjs report --in results.jsonl [--md report.md] \
//     [--baseline file [--update-baseline]]
//
// The CI lane drives this through lane.sh: a fixed selection
// (stride/cap) against a pinned suite SHA, checked against the
// checked-in expectations file.

import { spawn } from "node:child_process";
import * as fs from "node:fs";
import * as path from "node:path";
import * as os from "node:os";
import { pathToFileURL } from "node:url";

// ---------- frontmatter ----------
// Minimal parser for the YAML subset test262 frontmatter actually uses:
// scalar keys, inline lists [a, b], dash lists, and the one-level-nested
// `negative:` block.  `info: |` bodies are skipped by indentation.
export function parseFrontmatter(src) {
    const m = src.match(/\/\*---([\s\S]*?)---\*\//);
    const meta = { includes: [], flags: [], features: [], negative: null };
    if (!m) return meta;
    const lines = m[1].split("\n");
    let i = 0;
    const listKeys = new Set(["includes", "flags", "features"]);
    while (i < lines.length) {
        const line = lines[i];
        const km = line.match(/^([A-Za-z_][\w-]*):\s*(.*)$/);
        if (!km) {
            i++;
            continue;
        }
        const key = km[1];
        let val = km[2].trim();
        i++;
        if (key === "negative") {
            const neg = {};
            while (i < lines.length && /^\s+[\w-]+:/.test(lines[i])) {
                const nm = lines[i].match(/^\s+([\w-]+):\s*(.*)$/);
                neg[nm[1]] = nm[2].trim();
                i++;
            }
            meta.negative = neg;
        } else if (listKeys.has(key)) {
            let items = [];
            if (val.startsWith("[")) {
                items = val
                    .replace(/^\[|\]$/g, "")
                    .split(",")
                    .map((s) => s.trim())
                    .filter(Boolean);
            } else {
                while (i < lines.length && /^\s+-\s/.test(lines[i])) {
                    items.push(lines[i].replace(/^\s+-\s*/, "").trim());
                    i++;
                }
            }
            meta[key] = items;
        } else if (val === "|" || val === ">") {
            while (i < lines.length && (/^\s\s+/.test(lines[i]) || lines[i].trim() === "")) i++;
        }
    }
    return meta;
}

// ---------- selection ----------
function collectTests(suiteDir, capBuiltins, strideLanguage = 1) {
    const tests = [];
    const walk = (dir, cb) => {
        for (const ent of fs.readdirSync(dir, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name))) {
            const p = path.join(dir, ent.name);
            if (ent.isDirectory()) walk(p, cb);
            else cb(p);
        }
    };
    const isTest = (p) => p.endsWith(".js") && !p.includes("_FIXTURE");

    // language: every strideLanguage-th test of the sorted walk (1 = all).
    // The walk is depth-first sorted, so a global stride samples every
    // directory proportionally — the lane's knob for fitting a CI budget.
    // annexB/language rides the same stride: Annex B is normative for
    // web-facing engines, so its extensions belong in the denominator.
    let li = 0;
    for (const root of [["test", "language"], ["test", "annexB", "language"]]) {
        walk(path.join(suiteDir, ...root), (p) => {
            if (isTest(p) && li++ % strideLanguage === 0) tests.push(p);
        });
    }
    walk(path.join(suiteDir, "test", "harness"), (p) => {
        if (isTest(p)) tests.push(p);
    });
    // built-ins (and their Annex B extensions): stratified — first N
    // tests of every leaf directory, so every constructor/method is
    // probed without the full 24k volume.
    const perDir = new Map();
    for (const root of [["test", "built-ins"], ["test", "annexB", "built-ins"]]) {
        walk(path.join(suiteDir, ...root), (p) => {
            if (!isTest(p)) return;
            const d = path.dirname(p);
            const got = perDir.get(d) || 0;
            if (got < capBuiltins) {
                perDir.set(d, got + 1);
                tests.push(p);
            }
        });
    }
    return tests;
}

// ---------- AOT viability ----------
// Tests no ahead-of-time engine can pass, whatever echojs implements:
// they need a compiler at run time (`eval`, the Function constructor,
// dynamic `import()`) or a host hook that has no AOT meaning (a second
// realm, an agent).  Classified before compiling — each would otherwise
// cost a compile+link to reach a foregone failure — and reported as
// `skip-unsupported`, so the pass rate reads "of what an AOT engine
// could conceivably pass".
const NEEDS_COMPILER = /(^|[^.\w])(eval|Function)\s*\(/;
const NEEDS_AGENT = /\$262\s*\.\s*agent/;
// `with` is dynamic scope: every name inside the block resolves against
// a runtime object, which is the same property eval has — bindings that
// cannot be known at compile time.  Anchored to statement position
// (line start) because a bare word-boundary match drowns in prose —
// assertion messages ("called with (undefined, ...)"), `with` as a
// method name (arr.with(i, v), get with()).  A same-line `else with`
// slips through, which errs the safe way: a missed skip leaves a
// failing test in the denominator rather than hiding a passing one.
const NEEDS_WITH = /^[ \t]*with\s*\(/m;
const UNSUPPORTED_FEATURES = new Set(["cross-realm", "ShadowRealm", "dynamic-import"]);
// whole trees devoted to eval/with semantics; their tests reach the
// construct through indirection the source scan below does not see
const UNSUPPORTED_DIRS = new Map([
    ["test/language/eval-code/", "eval"],
    ["test/annexB/language/eval-code/", "eval"],
    ["test/built-ins/eval/", "eval"],
    ["test/language/statements/with/", "with"],
]);

const stripFrontmatter = (src) => src.replace(/\/\*---[\s\S]*?---\*\//, "");

// harness files that themselves need a compiler or an agent —
// fnGlobalObject.js is `Function("return this;")()` — so including one
// disqualifies a test as surely as calling eval does.  Derived from the
// suite rather than listed, so it tracks the harness across SHA bumps.
let needyHarness = null;
function harnessNeedingHost(suiteDir) {
    if (needyHarness) return needyHarness;
    needyHarness = new Set();
    const dir = path.join(suiteDir, "harness");
    for (const f of fs.readdirSync(dir)) {
        if (!f.endsWith(".js")) continue;
        const body = stripFrontmatter(fs.readFileSync(path.join(dir, f), "utf8"));
        if (NEEDS_COMPILER.test(body) || NEEDS_AGENT.test(body)) needyHarness.add(f);
    }
    return needyHarness;
}

// null if the test is in scope; otherwise a short tag naming what it
// needs (recorded on the row, so the report can break the skips down).
// Exported for offline reclassification of recorded runs.
export function unsupportedReason(suiteDir, rel, src, meta) {
    if (meta.flags.includes("CanBlockIsFalse")) return "agent";
    for (const [d, tag] of UNSUPPORTED_DIRS) if (rel.startsWith(d)) return tag;
    const feat = meta.features.find((f) => UNSUPPORTED_FEATURES.has(f));
    if (feat) return feat;
    const inc = meta.includes.find((h) => harnessNeedingHost(suiteDir).has(h));
    if (inc) return `harness:${inc}`;
    const body = stripFrontmatter(src);
    if (NEEDS_COMPILER.test(body)) return "eval";
    if (NEEDS_AGENT.test(body)) return "agent";
    if (NEEDS_WITH.test(body)) return "with";
    return null;
}

// ---------- harness assembly ----------
function assembleSource(suiteDir, src, meta) {
    if (meta.flags.includes("raw")) return { source: src, strict: false };
    const strict = meta.flags.includes("onlyStrict");
    const harness = ["assert.js", "sta.js"];
    if (meta.flags.includes("async")) harness.push("doneprintHandle.js");
    harness.push(...meta.includes);
    const seen = new Set();
    let out = strict ? '"use strict";\n' : "";
    // doneprintHandle.js reports through print(), which is not an echojs
    // global — shim it so async completions are observable
    if (meta.flags.includes("async"))
        out += 'var print = typeof print === "function" ? print : function (m) { console.log(m); };\n';
    // partial $262 host object: the hooks echojs can honor (backed by
    // the __ejs runtime namespace); createRealm/evalScript/agent need a
    // second realm or eval and stay absent
    out += "var $262 = { global: globalThis, " +
        "gc: function () { __ejs.GC.collect(); }, " +
        "detachArrayBuffer: function (buffer) { __ejs.detachArrayBuffer(buffer); }, " +
        "destroy: function () {} };\n";
    for (const h of harness) {
        if (seen.has(h)) continue;
        seen.add(h);
        out += fs.readFileSync(path.join(suiteDir, "harness", h), "utf8") + "\n";
    }
    // echojs compiles the concatenation as one module, so the harness's
    // `function $DONE` is a module binding, not a global-object property —
    // asyncHelpers.asyncTest checks hasOwnProperty(globalThis, "$DONE")
    if (meta.flags.includes("async"))
        out += 'if (typeof $DONE === "function") globalThis.$DONE = $DONE;\n';
    return { source: out + src, strict };
}

// ---------- one test ----------
function run(cmd, args, opts, timeoutMs) {
    return new Promise((resolve) => {
        const child = spawn(cmd, args, { ...opts, stdio: ["ignore", "pipe", "pipe"] });
        let stdout = "",
            stderr = "",
            timedOut = false;
        const timer = setTimeout(() => {
            timedOut = true;
            child.kill("SIGKILL");
        }, timeoutMs);
        // cap captured output: a runaway test can spew gigabytes (an
        // unbounded += eventually dies on V8's max string length); only
        // the head is ever inspected
        const CAP = 1 << 20;
        child.stdout.on("data", (d) => {
            if (stdout.length < CAP) stdout += d;
        });
        child.stderr.on("data", (d) => {
            if (stderr.length < CAP) stderr += d;
        });
        child.on("close", (code, signal) => {
            clearTimeout(timer);
            resolve({ code, signal, stdout, stderr, timedOut });
        });
        child.on("error", (err) => {
            clearTimeout(timer);
            resolve({ code: -1, signal: null, stdout, stderr: String(err), timedOut });
        });
    });
}

const firstLine = (s) =>
    (s || "")
        .split("\n")
        .map((l) => l.trim())
        .filter((l) => l && !l.startsWith("ld: warning"))[0] || "";

async function runOne(cfg, testPath) {
    const rel = path.relative(cfg.suite, testPath);
    const src = fs.readFileSync(testPath, "utf8");
    const meta = parseFrontmatter(src);
    const base = {
        test: rel,
        features: meta.features,
        flags: meta.flags,
        neg: meta.negative ? `${meta.negative.phase}:${meta.negative.type}` : null,
    };
    const unsupported = unsupportedReason(cfg.suite, rel, src, meta);
    if (unsupported) return { ...base, status: "skip-unsupported", needs: unsupported };

    const isModule = meta.flags.includes("module");
    const tmp = fs.mkdtempSync(path.join(cfg.tmpRoot, "t262-"));
    try {
        const { source } = assembleSource(cfg.suite, src, meta);
        // module tests may import themselves by name — keep the original
        // basename for them
        const srcFile = path.join(tmp, isModule ? path.basename(testPath) : "test.js");
        fs.writeFileSync(srcFile, source);
        // module tests import sibling *_FIXTURE.js specifiers — stage the
        // test's directory's fixtures next to the assembled source so
        // file-relative resolution finds them
        if (isModule) {
            const dir = path.dirname(testPath);
            for (const f of fs.readdirSync(dir)) {
                if (f.endsWith("_FIXTURE.js"))
                    fs.copyFileSync(path.join(dir, f), path.join(tmp, f));
            }
        }
        const exe = path.join(tmp, "test.exe");
        const env = { ...process.env, TMPDIR: tmp, NO_COLOR: "1", TZ: "UTC" };
        delete env.FORCE_COLOR;

        // unflagged tests run with script-goal semantics (the probe's
        // sloppy-only simplification); module-flagged tests use the
        // compiler's module-goal default
        const goalArgs = isModule ? [] : ["--script"];
        const comp = await run("./ejs", ["--srcdir", "-q", ...goalArgs, "-o", exe, srcFile], { cwd: cfg.ejsRoot, env }, cfg.compileTimeoutMs);
        // resolution-phase failures surface at compile time in an AOT
        // world, same as parse/early
        const negParse = meta.negative && (meta.negative.phase === "parse" || meta.negative.phase === "early" || meta.negative.phase === "resolution");
        if (comp.timedOut) return { ...base, status: "compile-timeout" };
        if (comp.code !== 0 || !fs.existsSync(exe)) {
            if (negParse) return { ...base, status: "pass" };
            const err = firstLine(comp.stderr) || firstLine(comp.stdout);
            const isParse = /Line \d+:|SyntaxError|Unexpected token|Invalid regular expression/.test(err);
            return { ...base, status: isParse ? "fail-parse" : "fail-compile", err };
        }
        if (negParse) return { ...base, status: "fail-negative-parse-accepted" };

        const ex = await run(exe, [], { cwd: tmp, env }, cfg.runTimeoutMs);
        const negRuntime = meta.negative && meta.negative.phase === "runtime";
        if (ex.timedOut) return { ...base, status: "run-timeout" };
        if (ex.signal) return { ...base, status: "fail-crash", err: `signal ${ex.signal}: ${firstLine(ex.stderr)}` };
        if (negRuntime) {
            return ex.code !== 0
                ? { ...base, status: "pass" }
                : { ...base, status: "fail-negative-runtime-passed" };
        }
        if (ex.code !== 0) return { ...base, status: "fail-runtime", err: firstLine(ex.stderr) || firstLine(ex.stdout) };
        if (meta.flags.includes("async") && !ex.stdout.includes("Test262:AsyncTestComplete")) {
            return { ...base, status: "fail-async", err: firstLine(ex.stdout) };
        }
        return { ...base, status: "pass" };
    } catch (e) {
        return { ...base, status: "harness-error", err: String(e).slice(0, 300) };
    } finally {
        fs.rmSync(tmp, { recursive: true, force: true });
    }
}

// ---------- expectations ----------
// Format: one `<status> <test path>` line per expected-failing test,
// sorted by path; `skip <path>` means run it but ignore the outcome
// (environment-sensitive).  `#` lines are comments.  Checking is by
// membership — a listed test may fail any way; an unlisted one must
// pass.
function loadExpectations(file) {
    const map = new Map();
    if (!fs.existsSync(file)) return map;
    for (const line of fs.readFileSync(file, "utf8").split("\n")) {
        const l = line.trim();
        if (!l || l.startsWith("#")) continue;
        const sp = l.indexOf(" ");
        map.set(l.slice(sp + 1), l.slice(0, sp));
    }
    return map;
}

function checkExpectations(rows, expected) {
    const regressions = [], stale = [];
    for (const r of rows) {
        if (r.status.startsWith("skip-")) continue;
        const exp = expected.get(r.test);
        if (exp === "skip") continue;
        // harness-error is never baselined (writeExpectations excludes
        // it) — the runner itself broke, always a failure here
        if (r.status === "harness-error") {
            regressions.push(r);
            continue;
        }
        if (r.status === "pass") {
            if (exp) stale.push(r);
        } else if (!exp) {
            regressions.push(r);
        }
    }
    return { regressions, stale };
}

function writeExpectations(file, rows, prior) {
    const lines = [
        "# test262 lane expectations — tests expected to fail (membership is",
        "# what's checked; the recorded status is documentation).  `skip` =",
        "# environment-sensitive, outcome ignored.  Regenerate:",
        "#   test/test262/lane.sh --suite <checkout> --update",
        "",
    ];
    const entries = [];
    for (const [t, s] of prior) if (s === "skip") entries.push([t, "skip"]);
    const skips = new Set(entries.map(([t]) => t));
    for (const r of rows) {
        if (r.status === "pass" || r.status.startsWith("skip-") || r.status === "harness-error") continue;
        if (!skips.has(r.test)) entries.push([r.test, r.status]);
    }
    entries.sort((a, b) => (a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0));
    for (const [t, s] of entries) lines.push(`${s} ${t}`);
    fs.writeFileSync(file, lines.join("\n") + "\n");
    return entries.length;
}

// ---------- driver ----------
function parseArgs(argv) {
    const opts = {};
    for (let i = 0; i < argv.length; i++) {
        if (argv[i].startsWith("--")) {
            const key = argv[i].slice(2);
            if (i + 1 < argv.length && !argv[i + 1].startsWith("--")) opts[key] = argv[++i];
            else opts[key] = true;
        }
    }
    return opts;
}

async function cmdRun(opts) {
    const cfg = {
        suite: path.resolve(opts.suite),
        ejsRoot: path.resolve(opts.ejs),
        jobs: parseInt(opts.jobs || `${Math.max(2, os.cpus().length - 2)}`, 10),
        compileTimeoutMs: 90_000,
        runTimeoutMs: 15_000,
        tmpRoot: fs.mkdtempSync(path.join(os.tmpdir(), "test262-probe-")),
    };
    if (!fs.existsSync(path.join(cfg.ejsRoot, "ejs"))) throw new Error(`no ./ejs in ${cfg.ejsRoot}`);
    // --cap-builtins all runs every built-ins test (the full suite);
    // an integer keeps the curated per-leaf-dir cap
    const cap = opts["cap-builtins"] === "all" ? Infinity : parseInt(opts["cap-builtins"] || "3", 10);
    let tests = collectTests(cfg.suite, cap, parseInt(opts["stride-language"] || "1", 10));
    if (opts.filter) tests = tests.filter((t) => t.includes(opts.filter));
    // --shard K/N runs slice K of N: the list is sorted so every shard
    // agrees on the partition regardless of machine or run
    if (opts.shard) {
        const [k, n] = String(opts.shard).split("/").map((x) => parseInt(x, 10));
        if (!(n > 0) || !(k >= 0 && k < n)) throw new Error(`bad --shard ${opts.shard} (want K/N, 0 <= K < N)`);
        tests = tests.slice().sort().filter((_t, i) => i % n === k);
    }
    const outPath = opts.out || "results.jsonl";
    const out = fs.createWriteStream(outPath);
    console.log(`${tests.length} tests, ${cfg.jobs} jobs -> ${outPath}`);

    let next = 0,
        done = 0;
    const t0 = Date.now();
    const counts = {};
    const rows = [];
    await Promise.all(
        Array.from({ length: cfg.jobs }, async () => {
            while (next < tests.length) {
                const t = tests[next++];
                const res = await runOne(cfg, t);
                counts[res.status] = (counts[res.status] || 0) + 1;
                rows.push(res);
                out.write(JSON.stringify(res) + "\n");
                if (++done % 250 === 0) {
                    const rate = done / ((Date.now() - t0) / 1000);
                    console.log(
                        `${done}/${tests.length} (${rate.toFixed(1)}/s, eta ${Math.round((tests.length - done) / rate / 60)}m) ${JSON.stringify(counts)}`
                    );
                }
            }
        })
    );
    out.end();
    fs.rmSync(cfg.tmpRoot, { recursive: true, force: true });
    console.log("done:", JSON.stringify(counts, null, 1));

    if (!opts.expectations) return;
    const expected = loadExpectations(opts.expectations);
    if (opts["update-expectations"]) {
        const n = writeExpectations(opts.expectations, rows, expected);
        console.log(`wrote ${opts.expectations}: ${n} expected failures`);
        return;
    }
    const { regressions, stale } = checkExpectations(rows, expected);
    for (const r of regressions) console.log(`REGRESSION ${r.status} ${r.test}${r.err ? ` — ${r.err}` : ""}`);
    for (const r of stale) console.log(`STALE (now passes) ${r.test}`);
    if (stale.length)
        console.log(`${stale.length} expected failure(s) now pass — regenerate with --update-expectations`);
    if (regressions.length || stale.length) {
        console.log(`expectations check FAILED: ${regressions.length} regression(s), ${stale.length} stale`);
        process.exitCode = 1;
    } else {
        console.log(`expectations check OK (${expected.size} expected failures)`);
    }
}

function cmdReport(opts) {
    const rows = fs
        .readFileSync(opts.in, "utf8")
        .split("\n")
        .filter(Boolean)
        .map((l) => JSON.parse(l));
    // membership is tested per row per feature below — a Set, not the
    // array, or this is quadratic at full-suite volume
    const failing = new Set(rows.filter((r) => r.status.startsWith("fail-") || r.status.endsWith("-timeout")));
    const byStatus = {};
    for (const r of rows) byStatus[r.status] = (byStatus[r.status] || 0) + 1;

    // the headline: pass rate over what was actually evaluated (skips are
    // out-of-scope tests, not failures — see the AOT viability section)
    const evaluated = rows.filter((r) => !r.status.startsWith("skip")).length;
    const passed = byStatus.pass || 0;
    const skipped = rows.length - evaluated;
    const needs = new Map();
    for (const r of rows) if (r.needs) needs.set(r.needs, (needs.get(r.needs) || 0) + 1);

    // per-feature failure counts — the prioritized feature list
    const featFail = new Map(), featTotal = new Map();
    for (const r of rows) {
        for (const f of r.features || []) {
            featTotal.set(f, (featTotal.get(f) || 0) + 1);
            if (failing.has(r)) featFail.set(f, (featFail.get(f) || 0) + 1);
        }
    }
    // per-area pass rates (top two path components)
    const area = new Map();
    for (const r of rows) {
        if (r.status.startsWith("skip")) continue;
        const key = r.test.split("/").slice(1, 3).join("/");
        const a = area.get(key) || { pass: 0, total: 0 };
        a.total++;
        if (r.status === "pass") a.pass++;
        area.set(key, a);
    }
    // error signatures
    const sig = new Map();
    for (const r of failing) {
        const s = (r.err || "(no message)")
            .replace(/\S*test\.js:?\s*/g, "")
            .replace(/\d+/g, "N")
            .slice(0, 100);
        sig.set(s, (sig.get(s) || 0) + 1);
    }

    const lines = [];
    lines.push(`# test262 probe report`, "");
    lines.push(`**pass ${passed}/${evaluated} (${((100 * passed) / evaluated).toFixed(1)}%)**`, "");
    lines.push(`${rows.length} selected, ${skipped} skipped as out of scope for AOT`, "");
    if (needs.size)
        lines.push(
            "skipped by need: " + [...needs.entries()].sort((a, b) => b[1] - a[1]).map(([k, n]) => `${k} ${n}`).join(", "),
            ""
        );
    lines.push(`## By status`, "");
    for (const [k, v] of Object.entries(byStatus).sort((a, b) => b[1] - a[1])) lines.push(`- ${k}: ${v}`);
    lines.push("", `## Failures by feature (prioritized)`, "");
    lines.push(`| feature | fail | total |`, `|---|---|---|`);
    for (const [f, n] of [...featFail.entries()].sort((a, b) => b[1] - a[1])) lines.push(`| ${f} | ${n} | ${featTotal.get(f)} |`);
    lines.push("", `## Pass rate by area`, "");
    lines.push(`| area | pass | total | rate |`, `|---|---|---|---|`);
    for (const [k, a] of [...area.entries()].sort())
        lines.push(`| ${k} | ${a.pass} | ${a.total} | ${((100 * a.pass) / a.total).toFixed(1)}% |`);
    lines.push("", `## Top error signatures`, "");
    for (const [s, n] of [...sig.entries()].sort((a, b) => b[1] - a[1]).slice(0, 40)) lines.push(`- ${n}× \`${s}\``);
    if (opts.baseline) lines.push("", ...checkBaseline(opts, { evaluated, passed }));
    const md = lines.join("\n") + "\n";
    if (opts.md) fs.writeFileSync(opts.md, md);
    else process.stdout.write(md);
}

// ---------- baseline ----------
// The full-suite ratchet.  Expectations-per-test is the lane's contract
// and does not scale to 45k rows, so the full run holds two numbers: how
// many tests were evaluated (coverage must not shrink — a lost shard or
// a selection mistake shows up here) and how many passed (conformance
// must not go backwards).  `tolerance` absorbs the odd loaded-runner
// timeout; raise it if CI proves noisy, and regenerate after real work
// with --update-baseline.
function checkBaseline(opts, { evaluated, passed }) {
    const file = opts.baseline;
    const now = { evaluated, pass: passed, tolerance: 0 };
    if (opts["update-baseline"]) {
        const prior = fs.existsSync(file) ? JSON.parse(fs.readFileSync(file, "utf8")) : {};
        now.tolerance = prior.tolerance ?? 0;
        fs.writeFileSync(file, JSON.stringify(now, null, 4) + "\n");
        return [`## Baseline`, "", `wrote ${file}: ${JSON.stringify(now)}`];
    }
    if (!fs.existsSync(file)) {
        return [
            `## Baseline`,
            "",
            `no ${file} yet — nothing to compare against.  To start the ratchet,`,
            "commit this:",
            "",
            "```json",
            JSON.stringify(now, null, 4),
            "```",
        ];
    }
    const base = JSON.parse(fs.readFileSync(file, "utf8"));
    const tol = base.tolerance ?? 0;
    const out = [`## Baseline`, "", `baseline ${base.pass}/${base.evaluated} (tolerance ${tol})`, ""];
    const fails = [];
    if (evaluated < base.evaluated)
        fails.push(`coverage shrank: evaluated ${evaluated} < baseline ${base.evaluated} — a shard or selection is missing tests`);
    if (passed < base.pass - tol) fails.push(`conformance regressed: pass ${passed} < baseline ${base.pass} - ${tol}`);
    for (const f of fails) {
        console.error(f);
        out.push(`- FAIL ${f}`);
    }
    if (fails.length) process.exitCode = 1;
    else out.push(`- OK (pass ${passed - base.pass >= 0 ? "+" : ""}${passed - base.pass} vs baseline)`);
    if (passed > base.pass)
        out.push(`- ${passed - base.pass} more passing than the baseline — regenerate it with --update-baseline`);
    return out;
}

// CLI dispatch only when run directly — the classifier exports above
// are importable without side effects (offline reclassification)
const isMain = process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href;
if (isMain) {
    const [cmd, ...rest] = process.argv.slice(2);
    const opts = parseArgs(rest);
    if (cmd === "run") await cmdRun(opts);
    else if (cmd === "report") cmdReport(opts);
    else {
        console.error("usage: run-test262.mjs run|report [options]  (see file header)");
        process.exit(2);
    }
}

