// generator: none

class MyArray extends Array {
    constructor() {
        super();
    }

    pushTwice(x) {
        this.push(x);
        this.push(x);
    }

    get [Symbol.toStringTag]() { return "MyOwnArray"; }
}

var m = new MyArray();

console.log(Object.prototype.toString.call(m));

m.pushTwice(5);

console.log(m);
console.log(m.length);
