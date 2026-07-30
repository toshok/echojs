#!/usr/bin/env node
// Exec shim for the npm wrapper (release-P2).  node realpaths the main
// module, so __dirname is the package's true location even when npm
// invokes this through the node_modules/.bin symlink — and the driver
// binary therefore sees an argv[0] it can resolve include/ and lib/
// against (it does not chase symlinks itself).
"use strict";

const path = require("path");
const fs = require("fs");
const { spawnSync } = require("child_process");

const exe = path.join(__dirname, "..", "dist", "bin", "ejs");
if (!fs.existsSync(exe)) {
    console.error("ejs: native toolchain missing — the echojs postinstall did not run or failed;");
    console.error("     reinstall the package (npm rebuild echojs) and check its output.");
    process.exit(1);
}

const r = spawnSync(exe, process.argv.slice(2), { stdio: "inherit" });
if (r.error) {
    console.error(`ejs: failed to run ${exe}: ${r.error.message}`);
    process.exit(1);
}
process.exit(r.status === null ? 1 : r.status);
