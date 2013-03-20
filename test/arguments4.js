
function f(a, b, c, d) {
  console.log (this.toString());
  console.log (a);
  console.log (b);
  console.log (c);
  console.log (d);
}

function g() {
  f.apply ({toString: function () { return "yo"; }}, [1].concat(Array.prototype.slice.call(arguments)));
}

g(2,3,4);
