var x = "hello world";

console.log(x);
(function () {
    var x = "goodbye world";
    console.log(x);
    return x;
})();
console.log(x);
