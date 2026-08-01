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
//     [--jobs N] [--cap-builtins 3] [--stride-language 1] [--filter substr] \
//     [--out results.jsonl] [--expectations file [--update-expectations]]
//
//   node test/test262/run-test262.mjs report --in results.jsonl [--md report.md]
//
// The CI lane (language-P4) drives this through lane.sh: a fixed
// selection (stride/cap) against a pinned suite SHA, checked against
// the checked-in expectations file.

import { spawn } from "node:child_process";
import * as fs from "node:fs";
import * as path from "node:path";
import * as os from "node:os";

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
    let li = 0;
    walk(path.join(suiteDir, "test", "language"), (p) => {
        if (isTest(p) && li++ % strideLanguage === 0) tests.push(p);
    });
    walk(path.join(suiteDir, "test", "harness"), (p) => {
        if (isTest(p)) tests.push(p);
    });
    // built-ins: stratified — first N tests of every leaf directory, so
    // every constructor/method is probed without the full 24k volume.
    const perDir = new Map();
    walk(path.join(suiteDir, "test", "built-ins"), (p) => {
        if (!isTest(p)) return;
        const d = path.dirname(p);
        const got = perDir.get(d) || 0;
        if (got < capBuiltins) {
            perDir.set(d, got + 1);
            tests.push(p);
        }
    });
    return tests;
}

// ---------- harness assembly ----------
function assembleSource(suiteDir, testPath, meta) {
    const src = fs.readFileSync(testPath, "utf8");
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
        child.stdout.on("data", (d) => (stdout += d));
        child.stderr.on("data", (d) => (stderr += d));
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
    const meta = parseFrontmatter(fs.readFileSync(testPath, "utf8"));
    const base = {
        test: rel,
        features: meta.features,
        flags: meta.flags,
        neg: meta.negative ? `${meta.negative.phase}:${meta.negative.type}` : null,
    };
    if (meta.flags.includes("module") || (meta.negative && meta.negative.phase === "resolution")) {
        return { ...base, status: "skip-module" };
    }
    if (meta.flags.includes("CanBlockIsFalse")) return { ...base, status: "skip-agent" };

    const tmp = fs.mkdtempSync(path.join(cfg.tmpRoot, "t262-"));
    try {
        const { source } = assembleSource(cfg.suite, testPath, meta);
        const srcFile = path.join(tmp, "test.js");
        fs.writeFileSync(srcFile, source);
        const exe = path.join(tmp, "test.exe");
        const env = { ...process.env, TMPDIR: tmp, NO_COLOR: "1", TZ: "UTC" };
        delete env.FORCE_COLOR;

        const comp = await run("./ejs", ["--srcdir", "-q", "-o", exe, srcFile], { cwd: cfg.ejsRoot, env }, cfg.compileTimeoutMs);
        const negParse = meta.negative && (meta.negative.phase === "parse" || meta.negative.phase === "early");
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
    let tests = collectTests(
        cfg.suite,
        parseInt(opts["cap-builtins"] || "3", 10),
        parseInt(opts["stride-language"] || "1", 10)
    );
    if (opts.filter) tests = tests.filter((t) => t.includes(opts.filter));
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
    const failing = rows.filter((r) => r.status.startsWith("fail-") || r.status.endsWith("-timeout"));
    const byStatus = {};
    for (const r of rows) byStatus[r.status] = (byStatus[r.status] || 0) + 1;

    // per-feature failure counts — the prioritized feature list
    const featFail = new Map(), featTotal = new Map();
    for (const r of rows) {
        for (const f of r.features || []) {
            featTotal.set(f, (featTotal.get(f) || 0) + 1);
            if (failing.includes(r)) featFail.set(f, (featFail.get(f) || 0) + 1);
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
    lines.push(`total: ${rows.length}`, "");
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
    const md = lines.join("\n") + "\n";
    if (opts.md) fs.writeFileSync(opts.md, md);
    else process.stdout.write(md);
}

const [cmd, ...rest] = process.argv.slice(2);
const opts = parseArgs(rest);
if (cmd === "run") await cmdRun(opts);
else if (cmd === "report") cmdReport(opts);
else {
    console.error("usage: run-test262.mjs run|report [options]  (see file header)");
    process.exit(2);
}
