// generator: babel-node
// "yield operator precedence" from kangax

var passed;
function* generator() {
    passed = yield 0 ? true : false;
}
var iterator = generator();
iterator.next();
iterator.next(true);
console.log(passed);
