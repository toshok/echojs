// generator: babel-node

var x = "y";
function foo() {
    return { [x]: 1 };
}

console.log(foo()[x] === 1);
