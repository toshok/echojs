// generator: babel-node

var a = Array(5);
for (var i = 0; i < 5; i++) {
    let i_ = i;
    a[i] = function () {
        console.log("hello world " + i_);
    };
}
for (i = 0; i < 5; i++) {
    a[i]();
}
