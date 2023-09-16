function test1() {
    return 5;
}
function test2() {
    return new Number(5);
}

console.log(new test1().toString());
console.log(new test2().toString());
