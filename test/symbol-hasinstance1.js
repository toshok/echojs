// generator: none

var a = 2,
    b = function () {};
b[Symbol.hasInstance] = function () {
    a = 4;
    return false;
};
({}) instanceof b;
console.log(a === 4);
