// generator: babel-node
// "computed shorthand generators" from kangax

var garply = "generator";
var o = {
    *[garply]() {
        yield 5;
        yield 6;
    },
};
var iterator = o.generator();
var item = iterator.next();
var passed = item.value === 5 && item.done === false;
item = iterator.next();
passed &= item.value === 6 && item.done === false;
item = iterator.next();
passed &= item.value === undefined && item.done === true;
console.log(passed);
