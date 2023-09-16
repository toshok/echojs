// regression test for the broken way the compiler was generating object literals
// (where if the object literal had a property key that also existed on
// Object.prototype, it would overwrite the Object.prototype property)

var a = {};
var b = {};

Object.prototype.myFunc = function () {
    console.log("yes way!");
};

a.myFunc();

var c = {
    myFunc: function () {
        console.log("no way!");
    },
};

c.myFunc();

b.myFunc();
