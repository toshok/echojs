function F() {
    this.hi = "hello from echo-js";
    this.sayHi = function () {
        console.log(this.hi);
    };
}

var f = new F();
f.sayHi();
