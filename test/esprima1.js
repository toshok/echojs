esprima = require ('esprima');
escodegen = require ('escodegen');

var str = "Set.prototype.member = function (el) { return hasOwn.call(this.set, el); };"

console.log (JSON.stringify (esprima.parse(str)));

