function foo(x) {
    console.log("before");
    switch (x) {
        case 0:
            console.log("hello");
            break;
        case 1:
            console.log("world.");
            break;
        case 2:
            console.log("goodbye");
            break;
        default:
            console.log("world.");
            break;
    }
    console.log("after");
}

foo(0);
foo(1);
foo(2);
foo(3);
