outer: while (true) {
    try {
        inner: while (true) {
            try {
                break outer;
            } catch (e) {
                console.log("inner catch");
            }
        }
    } finally {
        console.log("outer finally");
    }
}
console.log("done");
