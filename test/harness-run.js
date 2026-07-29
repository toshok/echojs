// node-side driver for expected-output generation (runtime-P3).
// Usage: node|babel-node harness-run.js <test.js>
// Installs the harness console shim, then runs the test — the exact
// mirror of the import wrapper tester.js compiles on the ejs side.
// Under babel-node the register hook transpiles the required test, which
// is how import-syntax tests generate.
require("./harness-console-shim.js");
require(require("path").resolve(process.argv[2]));
