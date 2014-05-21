
class MyArray extends Array {
    constructor() {
        super();
    }

    pushTwice(x) {
        this.push(x);
        this.push(x);
    }
}

// ejs doesn't support computed class properties yet, so we need to put this outside the class definition
MyArray.prototype[Symbol.toStringTag] = "MyOwnArray";

var m = new MyArray();

console.log(Object.prototype.toString.call(m));

m.pushTwice(5);

console.log(m);
console.log(m.length);
