function foo() {
    try {
        try {
            return 40;
        } finally {
            return 42;
        }
    } finally {
        return 44;
    }
}

console.log(foo());
