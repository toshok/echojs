if (typeof console === "undefined") { console = { log: print }; }

function Foo() {
}

var foo = new Foo();

if (foo instanceof Foo)
  console.log ("hello world");
else
  console.log ("goodbye cruel world");
