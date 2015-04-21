// lexical "arguments" binding from kangax
var f = (function() { return z => arguments[0]; }(5));
console.log(f(6) === 5);
