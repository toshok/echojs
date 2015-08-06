// generator: babel-node
// skip-if: true
// revisit the esprima tests now that we have the es6 modules

import * as esprima   from '../external-deps/esprima/esprima-es6';
import * as escodegen from '../external-deps/escodegen/escodegen-es6';

var str = "Set.prototype.member = function (el) { return hasOwn.call(this.set, el); };"

console.log (escodegen.generate(esprima.parse(str)));

