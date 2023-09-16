function foo() {
    try {
        return 40;
    } finally {
        return 42;
    }
}

console.log(foo());
