if (typeof console === "undefined") { console = { log: print }; }

foo = {};
foo.bar = {};
foo.bar.baz = "hello world";

console.log (foo.bar.baz);
