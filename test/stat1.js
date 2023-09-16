// generator: none

import * as fs from "@node-compat/fs";

console.log(fs.statSync("stat1.js").isFile());
console.log(fs.statSync("stat1.js").isDirectory());
