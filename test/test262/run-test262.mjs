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
//     [--jobs N] [--cap-builtins 3] [--filter substr] [--out results.jsonl]
//
//   node test/test262/run-test262.mjs report --in results.jsonl [--md report.md]

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
function collectTests(suiteDir, capBuiltins) {
    const tests = [];
    const walk = (dir, cb) => {
        for (const ent of fs.readdirSync(dir, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name))) {
            const p = path.join(dir, ent.name);
            if (ent.isDirectory()) walk(p, cb);
            else cb(p);
        }
    };
    const isTest = (p) => p.endsWith(".js") && !p.includes("_FIXTURE");

    for (const area of ["language", "harness"]) {
        walk(path.join(suiteDir, "test", area), (p) => {
            if (isTest(p)) tests.push(p);
        });
    }
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
    for (const h of harness) {
        if (seen.has(h)) continue;
        seen.add(h);
        out += fs.readFileSync(path.join(suiteDir, "harness", h), "utf8") + "\n";
    }
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
    let tests = collectTests(cfg.suite, parseInt(opts["cap-builtins"] || "3", 10));
    if (opts.filter) tests = tests.filter((t) => t.includes(opts.filter));
    const outPath = opts.out || "results.jsonl";
    const out = fs.createWriteStream(outPath);
    console.log(`${tests.length} tests, ${cfg.jobs} jobs -> ${outPath}`);

    let next = 0,
        done = 0;
    const t0 = Date.now();
    const counts = {};
    await Promise.all(
        Array.from({ length: cfg.jobs }, async () => {
            while (next < tests.length) {
                const t = tests[next++];
                const res = await runOne(cfg, t);
                counts[res.status] = (counts[res.status] || 0) + 1;
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
