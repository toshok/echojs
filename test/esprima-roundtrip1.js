esprima = require ('esprima');
escodegen = require ('escodegen');

var str = "Set.prototype.member = function (el) { return hasOwn.call(this.set, el); };"

console.warn (escodegen.generate(esprima.parse(str)));

