if (typeof console === "undefined") { console = { log: print }; }

var x = "hello world";

console.log (x);
(function() {
  var x = "goodbye world";
  console.log (x);
})();
console.log (x);
