if (typeof(console) == "object") var print = console.log;

var a = {b: 5};

Object.defineProperty (a, "b", {get: function() { return 15; } });

console.log (a.b);
