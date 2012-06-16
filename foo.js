function add2(n) {
  var o = n;
  {
    var p = o+2;
  }

  p = o + 4;
  return p;
}

function fib(n) {
  if (n === 0 || n === 1)
    return 1;
  else
    return fib(n-1) + fib(n-2);
}

/*
 * not yet - 'this' isn't supported, and neither is 'new', and assigning into property accessors ('dot')
 */
/*
function Fib() {
  this.gen = function(n) {
    if (n === 0 || n === 1)
      return 1;
    else
      return this.gen(n-1) + this.gen(n-2);
  };
}
*/

print("Fibonacci function");
print(fib(0));
print(fib(1));
print(fib(2));
print(fib(3));
print(fib(4));
print(fib(5));
print(fib(6));
print(fib(7));
print(fib(8));
print(fib(9));

print("function with local variables");
var add2arg=99;
print(add2(add2arg));

print ("Fibonacci object");
