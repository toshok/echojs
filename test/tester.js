#!/usr/bin/env node

var path = require('path'),
    os = require('os'),
    fs = require('fs'),
    glob = require('glob'),
    child_process = require('child_process'),
    spawn = child_process.spawn,
    exec = child_process.exec,
    colors = require('colors/safe'),
    temp = require('temp');

// maps from test_name -> properties as defined in the test file
var skip_ifs   = Object.create(null);   // `// skip-if: ...` an expression, evaled.  if true, ignore the test
var xfails     = Object.create(null);   // `// xfail: ...`   test is expected to fail.  ... is the reason
var generators = Object.create(null);   // `// generator: ...` ... is the executable used to generate expected output

var expected_names   = Object.create(null);
var expected_stdouts = Object.create(null);
var stdouts          = Object.create(null);

var failed_tests = [];

var compilers = [
    "../ejs",
    "../ejs.exe.stage1",
    "../ejs.exe.stage2"
];

var runloop_impl = require('../lib/generated/lib/host-config.js').RUNLOOP_IMPL;

var running_on_travis = process.env['TRAVIS_BUILD_NUMBER'] != null;

var stage_to_run = 0;

var test_threads = 4;

var result_types = {
    fail:  { str: "FAIL",  colorizer: colors.red.bold },
    xfail: { str: "xfail", colorizer: colors.yellow },
    xpass: { str: "ERROR", colorizer: colors.red.bold },
    pass:  { str: "pass",  colorizer: colors.green }
};

var fail_str = 'fail';
var xfail_str = 'xfail';
var xpass_str = 'xpass';
var pass_str  = 'pass';

function timerStart() {
  return process.hrtime();
}
// from http://stackoverflow.com/questions/10617070/how-to-measure-execution-time-of-javascript-code-with-callbacks
function getElapsed(start_time) {
    var elapsed = process.hrtime(start_time);
    var elapsed_ms = elapsed[0] * 1000 + elapsed[1] / 1000000;
    return elapsed_ms.toFixed(2); // 2 decimal places
}

function makeJustifierColumn(columns, leftJustify) {
    var spaces = Array(columns).join(" ");
    return function(str, transformer) {
	var padding = spaces.substr(0, columns - str.length);
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

var testColumn      = makeJustifierColumn(40, false);
var resultColumn    = makeJustifierColumn(5, true);    // maximum length of fail/xfail/xpass/pass
var timeColumn      = makeJustifierColumn(11, false);  // enough to hold "XXXXX.XX ms".
var errStringColumn = makeNoopColumn();

function writeOutput(test_name, result_type, elapsed, err_string) {
    var elapsed_str = elapsed == null ? "?" : elapsed;

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
    var test_stat = fs.statSync(test_file);
    try {
	var expected_stat = fs.statSync(expected_file);
	return test_stat.mtime.getTime() > expected_stat.mtime.getTime();
    }
    catch (e) {
	// XXX verify that e == ENOENT
	return true;
    }
}

function processOneTest(gen_expected, test, cb) {
    var test_name = path.basename(test);

    //if (!gen_expected) console.log("processOneTest(" + gen_expected + ", " + test_name + ")");
    if (skip_ifs[test_name]) {
	if (eval(skip_ifs[test_name])) {
	    //console.log("skipping " + test_name);
	    setTimeout(cb, 0);
	    return;
	}
    }

    if (gen_expected) {
	var expected_name = "./expected/" + test_name + ".expected-out";

	var should_generate = shouldGenerateExpectedOutput(test, expected_name);

	expected_names[test_name] = expected_name;
	var generator = generators[test_name] || "node";
	if (should_generate && generator !== "none") {
	    console.log("generating expected output for " + test_name + " using " + generator);

	    exec(generator + " " + test + " > " + expected_name, function (err, stdout) {
		if (err)
		    throw new Error(err);
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
            var start = timerStart();
	    var ccomp = spawn(compilers[stage_to_run], ["--srcdir", "--moduledir", "../node-compat", "--moduledir", "../ejs-llvm", test]);
	    ccomp.on("exit", function(code, errstring) {
		// XXX check code to make sure we were successful?
		var cexec = spawn("./" + test + ".exe");
		var test_stdout = "";
		var test_stderr = "";
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
                var elapsed = getElapsed(start);
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
    var i = 0;
    var e = tests.length;

    var num_outstanding = 0;

    var processTestCb = function() {
	//console.log("processTestCb");
	i ++;
	num_outstanding --;
	if (i >= e) {
	    //console.log("doing setTimeout");
	    if (num_outstanding == 0)
		setTimeout(cb, 0);
	    return;
	}

	num_outstanding++;
	processOneTest(gen_expected, tests[i], processTestCb);
    };

    for (var j = 0; j < test_threads; j ++) {
	processOneTest(gen_expected, tests[i++], processTestCb);

	num_outstanding++;
    }
}

function readTest(test) {
    var test_name = path.basename(test);
    var contents = fs.readFileSync(test).toString();
    var lines = contents.split('\n');

    // read the comments at the start, and pull out useful info
    for (var i = 0, e = lines.length; i < e; i ++) {
	var line = lines[i];
	if (line.indexOf("//") !== 0)
	    return;

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

var args = process.argv.slice(2);

var test_to_run = null;

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
	    var run_failed = failed_tests.length > 0;
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

