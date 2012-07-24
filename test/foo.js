/*
function add2(n) {
  var o = n;
  return o+2;
}

function fib(n) {
  if (n === 0 || n === 1)
    return 1;
  else
    return fib(n-1) + fib(n-2);
}
*/

function fib2(n) {
  var fibrec = function (fn) {
    if (n === 0 || n === 1)
      return 1;
    else
      return fibrec(n-1) + fibrec(n-2);
  };

  return fibrec(n);
}

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

/*
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
*/

print("Fibonacci function with closure");
print(fib2(0));
print(fib2(1));
print(fib2(2));
print(fib2(3));
print(fib2(4));
print(fib2(5));
print(fib2(6));
print(fib2(7));
print(fib2(8));
print(fib2(9));

/*
print("function with local variables");
var add2arg=99;
print(add2(add2arg));

print ("Fibonacci object");
*/