// generator: esm

// whole-module (toplevel-as-EIR) shapes: toplevel statements, captured
// toplevel locals, loop envs at toplevel, imports and exports.  runs and
// must agree under --ir, --ir --ir-toplevel, and the legacy pipeline.

import dflt, { K, inc, peek, counter } from "./eir-toplevel1/lib1";
import * as lib from "./eir-toplevel1/lib1";
import "./eir-toplevel1/lib1";

let greeting = "hello";
var count = 0;
function bump(n) { count += n; return count; }
console.log(greeting.length);
console.log(bump(2) + "," + bump(3));

let fns = [];
for (let i = 0; i < 3; i++) fns.push(function () { return i; });
console.log(fns.map(function (g) { return g(); }).join(","));

let holes = [, , "x"];
let seen = 0;
holes.forEach(function () { seen++; });
console.log(seen + "/" + holes.length + "/" + holes[2]);

console.log(K);
console.log(inc(2) + "," + inc(3));
console.log(peek());
console.log(lib.K + "/" + lib.peek());
console.log(dflt);
console.log(typeof bump === "function" ? bump.name : "?");
let local = K * 2;
export { local as doubled };
console.log(local);
