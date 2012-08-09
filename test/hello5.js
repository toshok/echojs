function outer_hello() { var x = "hello world"; return function () { print (x); }; }
var inner_hello = outer_hello();
inner_hello();
