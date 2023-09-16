function foo() {
    try {
        return 5;
    } catch (e) {
    } finally {
        console.log("hello");
    }
}

function bar() {
    try {
        return 10;
    } catch (e) {
    } finally {
        console.log("world");
    }
}

foo();
bar();
