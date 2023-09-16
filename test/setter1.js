var a = Object.create(null);

Object.defineProperty(a, "foo", {
    set: function (v) {
        console.log("foo being set to " + v);
    },
});

a.foo = 5;
a.foo = "hi";
