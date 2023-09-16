// generator: babel-node
// xfail: XXX

function test() {
    var x = "y",
        valueSet,
        obj = {
            get [x]() {
                return 1;
            },
            set [x](value) {
                valueSet = value;
            },
        };
    obj.y = "foo";
    return obj.y === 1 && valueSet === "foo";
}

console.log(test());
