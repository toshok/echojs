// generator: esm

// "sending" from kangax (xfailed under the esprima fork; the acorn parse
// of the array literal around the yields lowers correctly)

var sent;
function* generator() {
    sent = [yield 5, yield 6];
}
var iterator = generator();
iterator.next();
iterator.next("foo");
iterator.next("bar");
console.log(sent.toString());
console.log(sent[0] === "foo" && sent[1] === "bar");
