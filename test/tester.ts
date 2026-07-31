// The test-suite runner.  Compiled to tester.js by tsc (see
// tsconfig.json in this directory); buck-test-stage.sh does that when
// it stages the test tree, so the staged copy always runs from these
// sources.

import * as path from "path";
import * as os from "os";
import * as fs from "fs";
import { globSync } from "glob";
import * as child_process from "child_process";
import * as colors from "colors/safe";
import * as temp from "temp";

const spawn = child_process.spawn;
const exec = child_process.exec;

type Colorizer = (s: string) => string;

// maps from test_name -> properties as defined in the test file
const skip_ifs: Record<string, string> = Object.create(null); // `// skip-if: ...` an expression, evaled.  if true, ignore the test
const xfails: Record<string, string> = Object.create(null); // `// xfail: ...`   test is expected to fail.  ... is the reason
const generators: Record<string, string> = Object.create(null); // `// generator: ...` how expected output is generated (node | esm | none)

const expected_names: Record<string, string> = Object.create(null);
const expected_stdouts: Record<string, string> = Object.create(null);
const stdouts: Record<string, string> = Object.create(null);

const failed_tests: string[] = [];

// index here is the stage #.  0 = run it under node, 1 = run it with stage1, 2 = run it with stage2
const compilers = ["../ejs", "../ejs.exe.stage1", "../ejs.exe.stage2", "../ejs.exe.stage3"];

// eslint-disable-next-line @typescript-eslint/no-var-requires
const runloop_impl: string = require("../lib/generated/lib/host-config.js").RUNLOOP_IMPL;
// referenced from `// skip-if:` expressions, which eval in this scope
void runloop_impl;

// baselines must not depend on the timezone of the machine that generated
// them: local-time Date construction (date3.js) feeds the value-based
// serializer's UTC rendering, so generation and test runs both pin UTC
process.env.TZ = "UTC";

let platform_to_test: string | null = null;

let stage_to_run = 0;

let test_threads = 4;

// colors' chained styles (red.bold) aren't in its shipped types
const red_bold = (colors.red as unknown as { bold: Colorizer }).bold;

type ResultKind = "fail" | "xfail" | "xpass" | "pass";

const result_types: Record<ResultKind, { str: string; colorizer: Colorizer }> = {
    fail: { str: "FAIL", colorizer: red_bold },
    xfail: { str: "xfail", colorizer: colors.yellow },
    xpass: { str: "ERROR", colorizer: red_bold },
    pass: { str: "pass", colorizer: colors.green },
};

function timerStart(): [number, number] {
    return process.hrtime();
}
// from http://stackoverflow.com/questions/10617070/how-to-measure-execution-time-of-javascript-code-with-callbacks
function getElapsed(start_time: [number, number]): string {
    let elapsed = process.hrtime(start_time);
    let elapsed_ms = elapsed[0] * 1000 + elapsed[1] / 1000000;
    return elapsed_ms.toFixed(2); // 2 decimal places
}

function makeJustifierColumn(columns: number, leftJustify: boolean) {
    let spaces = Array(columns).join(" ");
    return function (str: string, transformer?: Colorizer): string {
        let padding = spaces.substr(0, columns - str.length);
        if (transformer) str = transformer(str);
        if (leftJustify) return str + padding;
        else return padding + str;
    };
}

function makeNoopColumn() {
    return function (x: string): string {
        return x;
    };
}

const testColumn = makeJustifierColumn(40, false);
const resultColumn = makeJustifierColumn(5, true); // maximum length of fail/xfail/xpass/pass
const timeColumn = makeJustifierColumn(11, false); // enough to hold "XXXXX.XX ms".
const errStringColumn = makeNoopColumn();

function writeOutput(
    test_name: string,
    result_type: ResultKind,
    elapsed: string | null,
    err_string?: string
): void {
    let elapsed_str = elapsed == null ? "?" : elapsed;

    console.log(
        testColumn(test_name),
        resultColumn(result_types[result_type].str, result_types[result_type].colorizer),
        timeColumn(elapsed_str + " ms"),
        errStringColumn(err_string ? err_string : "")
    );
}

function testFailure(
    test_name: string,
    err_string: string,
    elapsed: string | null,
    additional?: string
): void {
    writeOutput(test_name, "fail", elapsed, "(" + err_string + ")");
    console.log(additional);
    failed_tests.push(test_name);
}

function testUnexpectedPass(test_name: string, elapsed: string | null): void {
    writeOutput(test_name, "xpass", elapsed, "(unexpected pass)");
    failed_tests.push(test_name);
}

function testFailed(
    test_name: string,
    err_string: string,
    elapsed: string | null,
    additional?: string
): void {
    const xfail = xfails[test_name];
    if (xfail) {
        writeOutput(test_name, "xfail", elapsed, "(" + xfail + ")");
    } else {
        testFailure(test_name, err_string, elapsed, additional);
    }
}

function checkStdout(test_name: string, elapsed: string, cb: () => void): void {
    if (stdouts[test_name] != expected_stdouts[test_name]) {
        temp.open("ejstest-received", function (err, info) {
            fs.writeSync(info.fd, stdouts[test_name] ?? "");
            fs.close(info.fd, function () {
                exec(
                    "/usr/bin/diff -u " + expected_names[test_name] + " " + info.path,
                    function (err, stdout) {
                        testFailed(test_name, "stdout doesn't match", elapsed, stdout);
                        cb();
                    }
                );
            });
        });
    } else {
        if (xfails[test_name]) {
            testUnexpectedPass(test_name, elapsed);
        } else {
            writeOutput(test_name, "pass", elapsed);
        }

        setTimeout(cb, 0);
    }
}

// the value-based harness: tests generate and run with
// console.log replaced by the serializer in harness-console-shim.js, on
// both sides, so baselines assert on values, not on node's inspect format
const harness_shim = "harness-console-shim.js";
const harness_run = "harness-run.js";

function shouldGenerateExpectedOutput(test_file: string, expected_file: string): boolean {
    try {
        let expected_mtime = fs.statSync(expected_file).mtime.getTime();
        // the harness serializer contributes to the expected output too —
        // editing it must refresh every baseline
        let newest = fs.statSync(test_file).mtime.getTime();
        for (const dep of [harness_shim, harness_run]) {
            try {
                newest = Math.max(newest, fs.statSync(dep).mtime.getTime());
            } catch (e) {}
        }
        return newest > expected_mtime;
    } catch (e) {
        // XXX verify that e == ENOENT
        return true;
    }
}

// import-syntax tests (`// generator: esm`) can't run under plain node:
// their relative import specifiers are extensionless (the compiler's
// gather-imports requires import syntax, node's ESM loader requires
// extensions).  tsc transpiles the test and its relative-import closure
// to CommonJS in a scratch dir, and node runs the transpiled copy
// through the same harness-run driver.
function relativeImportClosure(test: string): string[] {
    const seen = new Set<string>();
    const files: string[] = [];
    const visit = function (file: string): void {
        const resolved = path.resolve(file);
        if (seen.has(resolved)) return;
        seen.add(resolved);
        files.push(resolved);
        const src = fs.readFileSync(resolved, "utf-8");
        const import_re = /^\s*(?:import|export)\b[^;]*?["']([^"']+)["']/gm;
        let m: RegExpExecArray | null;
        while ((m = import_re.exec(src)) !== null) {
            const spec = m[1];
            if (spec == null || spec[0] !== ".") continue;
            let dep = path.join(path.dirname(resolved), spec);
            if (!dep.endsWith(".js")) {
                // extensionless specifiers resolve like the compiler's:
                // file first, then directory/index.js (modules6)
                if (fs.existsSync(dep + ".js")) dep += ".js";
                else dep = path.join(dep, "index.js");
            }
            visit(dep);
        }
    };
    visit(test);
    return files;
}

const tsc_bin = path.join(path.dirname(require.resolve("typescript/package.json")), "bin", "tsc");

function generateExpectedEsm(
    test: string,
    expected_name: string,
    cb: (err?: Error | null) => void
): void {
    const gen_tmpdir = fs.mkdtempSync(path.join(os.tmpdir(), "ejstest-esm-"));
    const cleanup = function (): void {
        try {
            fs.rmSync(gen_tmpdir, { recursive: true, force: true });
        } catch (e) {}
    };
    const closure = relativeImportClosure(test)
        .map((f) => '"' + f + '"')
        .join(" ");
    exec(
        'node "' +
            tsc_bin +
            '" --ignoreConfig --allowJs --target es2016 --module commonjs' +
            ' --esModuleInterop --outDir "' +
            gen_tmpdir +
            '" ' +
            closure,
        function (err) {
            if (err) {
                cleanup();
                cb(err);
                return;
            }
            // the harness files are plain ES5 CommonJS — they ride along
            // unconverted so generation runs the byte-exact serializer
            for (const f of [harness_shim, harness_run]) {
                fs.copyFileSync(f, path.join(gen_tmpdir, f));
            }
            const transpiled = path.join(gen_tmpdir, path.basename(test));
            exec(
                'node "' +
                    path.join(gen_tmpdir, harness_run) +
                    '" "' +
                    transpiled +
                    '" > ' +
                    expected_name,
                function (err) {
                    cleanup();
                    cb(err);
                }
            );
        }
    );
}

function processOneTest(gen_expected: boolean, test: string, cb: (err?: Error | null) => void): void {
    let test_name = path.basename(test);

    //if (!gen_expected) console.log("processOneTest(" + gen_expected + ", " + test_name + ")");
    const skip_if = skip_ifs[test_name];
    if (skip_if) {
        if (eval(skip_if)) {
            //console.log("skipping " + test_name);
            setTimeout(cb, 0);
            return;
        }
    }

    if (gen_expected) {
        const expected_name = "./expected/" + test_name + ".expected-out";

        const should_generate = shouldGenerateExpectedOutput(test, expected_name);

        expected_names[test_name] = expected_name;
        const generator = generators[test_name] || "node";
        if (should_generate && generator !== "none") {
            console.log("generating expected output for " + test_name + " using " + generator);

            const generated = function (err?: Error | null): void {
                if (err) {
                    cb(err);
                    return;
                }
                expected_stdouts[test_name] = fs.readFileSync(expected_name).toString();
                cb();
            };
            if (generator === "esm") {
                generateExpectedEsm(test, expected_name, generated);
            } else {
                exec(generator + " " + harness_run + " " + test + " > " + expected_name, generated);
            }
        } else {
            try {
                expected_stdouts[test_name] = fs.readFileSync(expected_name).toString();
            } catch (e) {
                setTimeout(() => cb(e as Error), 0);
                return;
            }
            setTimeout(cb, 0);
        }
        return;
    } else {
        try {
            const start = timerStart();
            const platform_target = platform_to_test ? ["--target", platform_to_test] : [];
            const extra_flags = process.env.EJS_EXTRA_FLAGS
                ? process.env.EJS_EXTRA_FLAGS.split(" ")
                : [];
            // generator:none tests keep the legacy path (raw stdout against
            // a checked-in baseline, no shim); everything else compiles a
            // generated wrapper that imports the console shim, then the
            // test — the mirror of harness-run.js on the node side
            let compile_target = test;
            let output_args: string[] = [];
            let wrapper_name: string | null = null;
            if (generators[test_name] !== "none") {
                wrapper_name = ".__wrap__." + test_name;
                const spec = "./" + test_name.replace(/\.js$/, "");
                fs.writeFileSync(
                    wrapper_name,
                    "// generated by tester.ts (value-based harness); deleted after compile\n" +
                        'import "./' + harness_shim.replace(/\.js$/, "") + '";\n' +
                        'import "' + spec + '";\n'
                );
                compile_target = "./" + wrapper_name;
                output_args = ["-o", test + ".exe"];
            }
            // per-test TMPDIR (the types-diff lane's lesson): every test
            // compile now includes the harness-console-shim module, and the
            // compiler's temp names are only unique within one process —
            // concurrent compiles sharing a TMPDIR would clobber each
            // other's shim .bc/.o
            const compile_tmpdir = fs.mkdtempSync(path.join(os.tmpdir(), "ejstest-compile-"));
            const removeWrapper = function (): void {
                if (wrapper_name != null) {
                    try {
                        fs.unlinkSync(wrapper_name);
                    } catch (e) {}
                    wrapper_name = null;
                }
                try {
                    fs.rmSync(compile_tmpdir, { recursive: true, force: true });
                } catch (e) {}
            };
            const compiler = compilers[stage_to_run];
            if (compiler == null) throw new Error("bad stage " + stage_to_run);
            const ccomp = spawn(
                compiler,
                platform_target.concat(extra_flags).concat(output_args).concat([
                    "--srcdir",
                    "--moduledir",
                    "../node-compat",
                    "--moduledir",
                    "../ejs-llvm",
                    compile_target,
                ]),
                { env: Object.assign({}, process.env, { TMPDIR: compile_tmpdir }) }
            );
            ccomp.on("exit", function (code) {
                removeWrapper();
                if (code !== 0) {
                    const elapsed = getElapsed(start);
                    testFailed(test_name, `compiler failed (exit code = ${code})`, elapsed);
                    cb();
                    return;
                }
                // XXX check code to make sure we were successful?
                if (platform_to_test === "sim") {
                    process.env["EJS_FORCE_STDOUT"] = "1";
                    process.env["DYLD_FRAMEWORK_PATH"] =
                        "/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk/System/Library/Frameworks:/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk/System/Library/PrivateFrameworks";
                    process.env["DYLD_LIBRARY_PATH"] =
                        "/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk/usr/lib:/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk/usr/lib/system:/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk/System/Library/PrivateFrameworks/FontServices.framework:/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk/System/Library/Frameworks/Accelerate.framework/Frameworks/vecLib.framework:/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk//System/Library/Frameworks/OpenGLES.framework";
                }

                const cexec = spawn("./" + test + ".exe");
                let test_stdout = "";
                cexec.on("close", function () {
                    stdouts[test_name] = test_stdout;

                    // XXX check code to make sure we were successful?

                    const elapsed = getElapsed(start);
                    checkStdout(test_name, elapsed, cb);
                });
                cexec.on("error", function (err) {
                    const elapsed = getElapsed(start);
                    testFailed(test_name, err.toString(), elapsed);
                    cb();
                });
                cexec.stdout.on("data", function (msg) {
                    test_stdout += msg;
                });
                cexec.stderr.on("data", function () {});
            });
            ccomp.on("error", function (err) {
                removeWrapper();
                const elapsed = getElapsed(start);
                testFailed(test_name, err.toString(), elapsed);
                cb();
                return;
            });
        } catch (e) {
            console.log(e);
            setTimeout(cb, 0);
            return;
        }
    }
}

function processTests(
    gen_expected: boolean,
    tests: string[],
    cb: (err?: Error | null) => void
): void {
    // read tests[i] BEFORE incrementing i: seeding i=test_threads and
    // bumping first silently skips the test at index test_threads
    let next = 0;
    let num_outstanding = 0;

    const launch = function (): void {
        while (num_outstanding < test_threads && next < tests.length) {
            const t = tests[next++];
            if (t == null) continue;
            num_outstanding++;
            processOneTest(gen_expected, t, function () {
                num_outstanding--;
                if (next >= tests.length && num_outstanding === 0) {
                    setTimeout(cb, 0);
                    return;
                }
                launch();
            });
        }
    };
    launch();
}

function readTest(test: string): void {
    const test_name = path.basename(test);
    const contents = fs.readFileSync(test).toString();
    const lines = contents.split("\n");

    // read the comments at the start, and pull out useful info
    for (let i = 0, e = lines.length; i < e; i++) {
        let line = lines[i];
        if (line == null || line.indexOf("//") !== 0) {
            return;
        }

        line = line.substr(2).trim();

        if (line.indexOf("skip-if:") === 0) {
            if (skip_ifs[test_name])
                throw new Error("test " + test + " already has a skip-if: directive");
            skip_ifs[test_name] = line.substr("skip-if:".length).trim();
        }

        if (line.indexOf("xfail:") === 0) {
            if (xfails[test_name])
                throw new Error("test " + test + " already has a xfail: directive");
            xfails[test_name] = line.substr("xfail:".length).trim();
        }

        if (line.indexOf("generator:") === 0) {
            if (generators[test_name])
                throw new Error("test " + test + " already has a generator: directive");
            generators[test_name] = line.substr("generator:".length).trim();
        }
    }
}

const args = process.argv.slice(2);

let test_to_run: string | null = null;

if (args[0] == "-p") {
    args.shift();
    const p = args.shift();
    if (p == null) {
        throw new Error("-p requires an argument [osx, sim]");
    }
    platform_to_test = p;
    if (platform_to_test !== "osx" && platform_to_test !== "sim") {
        throw new Error("-p requires an argument [osx, sim]");
    }
}

if (args[0] == "-s") {
    args.shift();
    const s = args.shift();
    if (s == null) throw new Error("-s requires an argument between 0 and 3");
    stage_to_run = parseInt(s);
    if (!(stage_to_run >= 0 && stage_to_run < compilers.length))
        throw new Error("-s requires an argument between 0 and 3");
}
if (args[0] == "-t") {
    args.shift();
    const t = args.shift();
    if (t == null) throw new Error("-t requires an argument (the test file to run)");
    test_to_run = t;
    test_threads = 1; // XXX workaround for a bug, but we also only need 1 thread when we're running 1 test
}

function runTests(tests: string[]): void {
    tests.forEach(readTest);

    if (tests.length == 1)
        console.log(
            "running " +
                tests[0] +
                " against stage " +
                stage_to_run +
                " (" +
                compilers[stage_to_run] +
                ")"
        );
    else
        console.log(
            "running " +
                tests.length +
                " tests against stage " +
                stage_to_run +
                " (" +
                compilers[stage_to_run] +
                ")"
        );

    processTests(true, tests, function (err) {
        if (err) {
            console.log(err);
            process.exit(1);
        }
        processTests(false, tests, function () {
            const run_failed = failed_tests.length > 0;
            if (run_failed) {
                console.log();
                console.log(testColumn(failed_tests.length + " failed tests"));
                console.log(testColumn("================"));
                failed_tests.forEach(function (t) {
                    console.log(testColumn(t));
                });
            }
            console.log(
                testColumn(" "),
                resultColumn("done", result_types[run_failed ? "fail" : "pass"].colorizer)
            );
            process.exit(run_failed ? -1 : 0);
        });
    });
}

if (test_to_run) {
    runTests([test_to_run]);
} else {
    // run all the tests

    let tests = globSync("./*+([0-9]).js");
    runTests(tests);
}
