// generator: none
// baseline checked in: babel-node can't run the external-deps esprima-es6
// ESM under babel-register (was silently unregenerable under the old
// harness too); the output is JSON.stringify of the AST, engine-neutral
// revisit the esprima tests now that we have the es6 modules

import * as esprima from "../external-deps/esprima/esprima-es6";
import * as escodegen from "../external-deps/escodegen/escodegen-es6";

var str = "Set.prototype.member = function (el) { return hasOwn.call(this.set, el); };";

console.log(JSON.stringify(esprima.parse(str)));
