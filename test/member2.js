if (typeof console === "undefined") { console = { log: print }; }

var baz = console.invalidfield;

foo = {};
foo.bar = {};
foo.bar.baz = "hello world";

console.log (foo.bar.baz);
