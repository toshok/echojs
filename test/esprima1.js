import esprima   from "esprima-es6";
import escodegen from "escodegen-es6";

var str = "Set.prototype.member = function (el) { return hasOwn.call(this.set, el); };";

console.log (JSON.stringify (esprima.parse(str)));

