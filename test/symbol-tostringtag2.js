// generator: babel-node

class Foo {
    get [Symbol.toStringTag] () {
	return "Fooooo";
    }
}

var a = new Foo();
console.log(a.toString());
