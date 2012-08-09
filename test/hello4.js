function outer_hello() { var x = "hello world"; return function () { print (x); }; }
outer_hello()();
