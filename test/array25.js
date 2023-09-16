// generator: babel-node

var arr = [1, 7, 11, "hola", 13.69];
var res = arr.findIndex(function (element, index, array) {
    return element == "hola";
});

console.log(res);

// courtesy of our MDN Documentation friends.
function isPrime(element, index, array) {
    var start = 2;
    while (start <= Math.sqrt(element)) if (element % start++ < 1) return false;

    return element > 1;
}

console.log([4, 6, 8, 12].findIndex(isPrime)); // -1, not found
console.log([4, 6, 7, 12].findIndex(isPrime)); // 2
