#!/usr/bin/env node

const path = require('path'),
    os = require('os'),
    fs = require('fs'),
    glob = require('glob'),
    child_process = require('child_process'),
    spawn = child_process.spawn,
    exec = child_process.exec,
    colors = require('colors/safe'),
    temp = require('temp');

// maps from test_name -> properties as defined in the test file
const skip_ifs   = Object.create(null);   // `// skip-if: ...` an expression, evaled.  if true, ignore the test
const xfails     = Object.create(null);   // `// xfail: ...`   test is expected to fail.  ... is the reason
const generators = Object.create(null);   // `// generator: ...` ... is the executable used to generate expected output

const expected_names   = Object.create(null);
const expected_stdouts = Object.create(null);
const stdouts          = Object.create(null);

const failed_tests = [];

const compilers = [
    "../ejs",
    "../ejs.exe.stage1",
    "../ejs.exe.stage2"
];

let runloop_impl = require('../lib/generated/lib/host-config.js').RUNLOOP_IMPL;

const running_on_travis = process.env['TRAVIS_BUILD_NUMBER'] != null;

let platform_to_test = null;

let stage_to_run = 0;

let test_threads = 4;

const result_types = {
    fail:  { str: "FAIL",  colorizer: colors.red.bold },
    xfail: { str: "xfail", colorizer: colors.yellow },
    xpass: { str: "ERROR", colorizer: colors.red.bold },
    pass:  { str: "pass",  colorizer: colors.green }
};

const fail_str = 'fail';
const xfail_str = 'xfail';
const xpass_str = 'xpass';
const pass_str  = 'pass';

function timerStart() {
  return process.hrtime();
}
// from http://stackoverflow.com/questions/10617070/how-to-measure-execution-time-of-javascript-code-with-callbacks
function getElapsed(start_time) {
    let elapsed = process.hrtime(start_time);
    let elapsed_ms = elapsed[0] * 1000 + elapsed[1] / 1000000;
    return elapsed_ms.toFixed(2); // 2 decimal places
}

function makeJustifierColumn(columns, leftJustify) {
    let spaces = Array(columns).join(" ");
    return function(str, transformer) {
        let padding = spaces.substr(0, columns - str.length);
        if (transformer)
            str = transformer(str);
        if (leftJustify)
            return str + padding;
        else
            return padding + str;
    };
}

function makeNoopColumn() {
    return function (x) { return x; };
}

const testColumn      = makeJustifierColumn(40, false);
const resultColumn    = makeJustifierColumn(5, true);    // maximum length of fail/xfail/xpass/pass
const timeColumn      = makeJustifierColumn(11, false);  // enough to hold "XXXXX.XX ms".
const errStringColumn = makeNoopColumn();

function writeOutput(test_name, result_type, elapsed, err_string) {
    let elapsed_str = elapsed == null ? "?" : elapsed;

    console.log(
        testColumn(test_name),
        resultColumn(result_types[result_type].str, result_types[result_type].colorizer),
        timeColumn(elapsed_str + " ms"),
        errStringColumn(err_string ? err_string : "")
    );
}

function testFailure(test_name, err_string, elapsed, additional) {
    writeOutput(test_name, fail_str, elapsed, "(" + err_string + ")");
    console.log(additional);
    failed_tests.push(test_name);
}

function testUnexpectedPass(test_name, elapsed) {
    writeOutput(test_name, xpass_str, elapsed, "(unexpected pass)");
    failed_tests.push(test_name);
}

function testFailed(test_name, err_string, elapsed, additional) {
    if (xfails[test_name]) {
        writeOutput(test_name, xfail_str, elapsed, "(" + xfails[test_name] + ")");
    }
    else {
        testFailure(test_name, err_string, elapsed, additional);
    }
}

function checkStdout(test_name, elapsed, cb) {
    if (stdouts[test_name] != expected_stdouts[test_name]) {
        temp.open("ejstest-received", function (err, info) {
            fs.writeSync(info.fd, stdouts[test_name]);
            fs.close(info.fd, function (err) {
                exec("/usr/bin/diff -u " + expected_names[test_name] + " " + info.path, function (err, stdout) {
                    testFailed(test_name, "stdout doesn't match", elapsed, stdout);
                    cb();
                });
            });
        });
    }
    else {
        if (xfails[test_name]) {
            testUnexpectedPass(test_name, elapsed);
        }
        else {
            writeOutput(test_name, pass_str, elapsed);
        }

        setTimeout(cb, 0);
    }
}

function shouldGenerateExpectedOutput(test_file, expected_file) {
    let test_stat = fs.statSync(test_file);
    try {
        let expected_stat = fs.statSync(expected_file);
        return test_stat.mtime.getTime() > expected_stat.mtime.getTime();
    }
    catch (e) {
        // XXX verify that e == ENOENT
        return true;
    }
}

function processOneTest(gen_expected, test, cb) {
    let test_name = path.basename(test);

    //if (!gen_expected) console.log("processOneTest(" + gen_expected + ", " + test_name + ")");
    if (skip_ifs[test_name]) {
        if (eval(skip_ifs[test_name])) {
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

            exec(generator + " " + test + " > " + expected_name, function (err, stdout) {
                if (err) {
                    throw new Error(err);
                }
                expected_stdouts[test_name] = fs.readFileSync(expected_name).toString();
                cb();
            });
        }
        else {
            expected_stdouts[test_name] = fs.readFileSync(expected_name).toString();
            setTimeout(cb, 0);
        }
        return;
    }
    else {
        try {
            const start = timerStart();
            const platform_target = platform_to_test ? ["--target", platform_to_test] : [];
            const ccomp = spawn(compilers[stage_to_run],  platform_target.concat(["--srcdir", "--moduledir", "../node-compat", "--moduledir", "../ejs-llvm", test]));
            ccomp.on("exit", function(code, errstring) {
                // XXX check code to make sure we were successful?
                let env;
                if (platform_to_test === 'sim') {
                    process.env['EJS_FORCE_STDOUT'] = '1';
                    process.env['DYLD_FRAMEWORK_PATH'] = '/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk/System/Library/Frameworks:/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk/System/Library/PrivateFrameworks';
                    process.env['DYLD_LIBRARY_PATH'] = '/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk/usr/lib:/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk/usr/lib/system:/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk/System/Library/PrivateFrameworks/FontServices.framework:/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk/System/Library/Frameworks/Accelerate.framework/Frameworks/vecLib.framework:/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk//System/Library/Frameworks/OpenGLES.framework';
                }

                const cexec = spawn("./" + test + ".exe");
                let test_stdout = "";
                let test_stderr = "";
                cexec.on("close", function(code, errstring) {
                    stdouts[test_name] = test_stdout;

                    // XXX check code to make sure we were successful?

                    var elapsed = getElapsed(start);
                    checkStdout(test_name, elapsed, cb);
                });
                cexec.on("error", function(err) {
                    var elapsed = getElapsed(start);
                    testFailed(test_name, err.toString(), elapsed);
                    cb();
                });
                cexec.stdout.on("data", function(msg) {
                    test_stdout += msg;
                });
                cexec.stderr.on("data", function(msg) {
                    test_stderr += msg;
                });
            });
            ccomp.on("error", function(err) {
                const elapsed = getElapsed(start);
                testFailed(test_name, err.toString(), elapsed);
                cb();
                return;
            });
        }
        catch (e) {
            console.log(e);
            setTimeout(cb, 0);
            return;
        }
    }
}

function processTests(gen_expected, tests, cb) {
    let i = 0;
    const e = tests.length;

    let num_outstanding = 0;

    const processTestCb = function() {
        //console.log("processTestCb");
        i ++;
        num_outstanding --;
        if (i >= e) {
            //console.log("doing setTimeout");
            if (num_outstanding == 0) {
                setTimeout(cb, 0);
            }
            return;
        }

        num_outstanding++;
        processOneTest(gen_expected, tests[i], processTestCb);
    };

    for (let j = 0; j < test_threads; j ++) {
        processOneTest(gen_expected, tests[i++], processTestCb);

        num_outstanding++;
    }
}

function readTest(test) {
    const test_name = path.basename(test);
    const contents = fs.readFileSync(test).toString();
    const lines = contents.split('\n');

    // read the comments at the start, and pull out useful info
    for (let i = 0, e = lines.length; i < e; i ++) {
        let line = lines[i];
        if (line.indexOf("//") !== 0) {
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

let test_to_run = null;

if (args[0] == '-p') {
    args.shift();
    if (args.length < 1) {
        throw new Error("-p requires an argument [osx, sim]");
    }
    platform_to_test = args.shift();
    if (platform_to_test !== 'osx' && platform_to_test !== 'sim') {
        throw new Error("-p requires an argument [osx, sim]");
    }
}

if (args[0] == '-s') {
    args.shift();
    if (args.length < 1)
        throw new Error("-s requires an argument between 0 and 2");
    stage_to_run = parseInt(args.shift());
    if (stage_to_run < 0 && stage_to_run > 2)
        throw new Error("-s requires an argument between 0 and 2");
}
if (args[0] == '-t') {
    args.shift();
    if (args.length < 1)
        throw new Error("-t requires an argument (the test file to run)");
    test_to_run = args.shift();
    test_threads = 1; // XXX workaround for a bug, but we also only need 1 thread when we're running 1 test
}

function runTests(tests) {
    tests.forEach(readTest);

    if (tests.length == 1)
        console.log("running " + tests[0] + " against stage " + stage_to_run + " (" + compilers[stage_to_run] + ")");
    else
        console.log("running " + tests.length + " tests against stage " + stage_to_run + " (" + compilers[stage_to_run] + ")");
        
    processTests(true, tests, function() {
        processTests(false, tests, function() {
            const run_failed = failed_tests.length > 0;
            if (run_failed > 0) {
                console.log();
                console.log(testColumn(failed_tests.length + " failed tests"));
                console.log(testColumn("================"));
                failed_tests.forEach(function (t) {
                    console.log(testColumn(t));
                });
            }
            console.log(testColumn(" "),
                        resultColumn("done", result_types[run_failed ? "fail" : "pass"].colorizer));
            process.exit(run_failed ? 1 : 0);
        });
    });
}

if (test_to_run) {
    runTests([test_to_run]);
}
else {
    // run all the tests

    glob("./*+([0-9]).js", function (err, tests) {
        runTests(tests);
    });
}

