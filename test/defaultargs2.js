// generator: babel-node

function foo(b, a = "hello") {
    console.log(a);
}

function bar(b1, b2) {
    foo(b1, b2);
}

bar(5, "goodbye");
bar(7);
