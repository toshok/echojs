var strings = ["world", "lovely person", "terrible person"];

var re = /%(\d)/;

console.log(
    "hello, %0".replace(re, function (match, index) {
        return strings[index];
    })
);
