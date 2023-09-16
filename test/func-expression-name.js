var f = function () {
    console.log("hello");
};
var o = {};

o.someFunc = function () {
    console.log("hello again");
};

f();
o.someFunc();
