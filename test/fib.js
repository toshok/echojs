function fib(n) {
    if (n === 0 || n === 1) return 1;
    else return fib(n - 1) + fib(n - 2);
}

function fib2(n) {
    var fibrec = function (fn) {
        if (fn === 0 || fn === 1) return 1;
        else return fibrec(fn - 1) + fibrec(fn - 2);
    };

    return fibrec(n);
}

//print = console.log;
console.log("Fibonacci function");
console.log(fib(0));
console.log(fib(1));
console.log(fib(2));
console.log(fib(3));
console.log(fib(4));
console.log(fib(5));
console.log(fib(6));
console.log(fib(7));
console.log(fib(8));
console.log(fib(9));

console.log("Fibonacci function with inner function");
console.log(fib2(0));
console.log(fib2(1));
console.log(fib2(2));
console.log(fib2(3));
console.log(fib2(4));
console.log(fib2(5));
console.log(fib2(6));
console.log(fib2(7));
console.log(fib2(8));
console.log(fib2(9));
