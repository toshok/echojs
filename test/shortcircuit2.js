function foo(a, b) {
    b || (b = a);
    console.log(b);
}

foo(5);
foo(5, "hi");
