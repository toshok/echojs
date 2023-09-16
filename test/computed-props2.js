// generator: none

class Foo {
    get [Symbol.toStringTag]() {
        return "Foo";
    }
    [Symbol.iterator]() {
        console.log("i should return an iterator");
        return [1, 2, 3][Symbol.iterator]();
    }
}

var f = new Foo();
console.log(Object.prototype.toString.call(f));

for (var x of f) {
    console.log(x);
}
