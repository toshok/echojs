var baz = console.invalidfield;

var foo = {};
foo.bar = {};
foo.bar.baz = "hello world";
foo.bar.fweep = function() { return 1; };

var arr = [0, 1, 2, 3, 4];
console.log (foo.bar.baz);
console.log (arr[foo.bar.fweep()]);
