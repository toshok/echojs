
function* foo(arg1) {
    console.log(1);
    yield 1;
    console.log(2);
    yield 2;
    console.log(3);
    yield 3;
    console.log(4);
    yield arg1;
    console.log(arg1);
}

let f = foo();

console.log (f.next().value);
console.log (f.next().value);
console.log (f.next().value);
console.log (f.next().value);
