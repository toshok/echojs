// xfail: we don't output numbers correctly (integers with exponent <= 20 should print out normally)

function fib(n) {
  var a = 1;
  var b = 0;
  var SZ = Math.pow(2,64);

  for(var i = 0; i < n; i++) {
      var t = b;
      b = (a+b)%SZ;
      a = t;
  }

  return b;
}

console.log (fib (50));
