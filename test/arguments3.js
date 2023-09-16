function foo(n) {
    console.log(n);
}

function bar() {
    foo.apply(null, arguments);
}

bar("hello world");
