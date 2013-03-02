esprima = require ('esprima');
escodegen = require ('escodegen');

var str = "Set.prototype.member = function (el) { return hasOwn.call(this.set, el); };"

console.log (escodegen.generate(esprima.parse(str, { loc: true })));
console.log ("---");
console.log (escodegen.generate(esprima.parse(str, { raw: true })));
