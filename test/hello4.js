if (typeof console === "undefined") { console = { log: print }; }

function outer_hello() { var x = "hello world"; return function () { console.log (x); }; }
outer_hello()();
