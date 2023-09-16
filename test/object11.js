if (typeof console == "object") var print = console.log;

var a = Object.create(null);

Object.defineProperty(a, "b", {
    get: function () {
        return 15;
    },
    set: function (v) {
        print("1: setting value to " + v);
    },
    configurable: true,
});

a.b = 10;

Object.defineProperty(a, "b", {
    get: function () {
        return 5;
    },
    set: function (v) {
        print("2: setting value to " + v);
    },
    configurable: true,
});

a.b = 10;

console.log(a.b);
