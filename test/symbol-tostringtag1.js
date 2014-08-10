var a = {};
a[Symbol.toStringTag] = "foo";
console.log( (a + "") === "[object foo]");
