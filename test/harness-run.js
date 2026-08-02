// node-side driver for expected-output generation.
// Usage: node harness-run.js <test.js>
// Installs the harness console shim, then runs the test — the exact
// mirror of the import wrapper tester.ts compiles on the ejs side.
// Import-syntax tests (`// generator: esm`) run through this too: the
// tester tsc-transpiles them into a scratch dir (a copy of this file
// and the shim ride along) — see generateExpectedEsm in tester.ts.
require("./harness-console-shim.js");
require(require("path").resolve(process.argv[2]));
