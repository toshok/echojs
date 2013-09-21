function sayIt(x,w) {
  console.log (x + w);
}

var x = "hello";
var world = " world";

sayIt (x, world);
(function() {
  var x = "goodbye";
  sayIt(x, world);
})();
sayIt (x, world);
