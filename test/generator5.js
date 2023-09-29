// generator: babel-node
// xfail: generator support isn't 100%

// "sending" from kangax

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
