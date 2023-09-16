function wrapFunction(fn) {
    return function () {
        console.log("before apply");
        fn.apply(null, ["hello world"]);
        console.log("after apply");
    };
}

var log = wrapFunction(console.log);
log("hello world");
