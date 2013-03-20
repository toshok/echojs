
function F() { }
F.prototype.toString = function() { return ""; };

(function () {
  console.log (F.prototype.toString());

  var f = {}
  console.log (f instanceof F);

  f = new F();
  console.log (f instanceof F);
})();
