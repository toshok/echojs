if (typeof console === "undefined") { console = { log: print }; }

var x = "hello world";

console.log (x);
var bye = (function() {
     var x = "goodbye world";
     return x;
})();
console.log (bye);
console.log (x);
