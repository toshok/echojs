if (typeof console === "undefined") { console = { log: print }; }

function outer_hello() { var x = "hello world"; function inner_hello() { console.log (x); } inner_hello(); }
outer_hello();
