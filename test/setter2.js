function f() {}

Object.defineProperty(f.prototype, "foo", {
    get: function () {
        console.log("returning foo");
        return this._foo;
    },
});

var a = new f();
a._foo = 10;

console.log(a.foo);
