outer: while (true) {
    try {
        inner: while (true) {
            try {
                break outer;
            } finally {
                console.log("inner finally");
            }
        }
    } finally {
        console.log("outer finally");
    }
}
console.log("done");
