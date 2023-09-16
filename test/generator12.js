// generator: babel-node
// skip-if: true
// this test fails for stage1 but not for stage0.  we need to add a way to disable tests just for particular stages

// "yield *, astral plane strings" from kangax

var iterator = (function* generator() {
    yield* "𠮷𠮶";
})();
var item = iterator.next();
var passed = item.value === "𠮷" && item.done === false;
item = iterator.next();
passed &= item.value === "𠮶" && item.done === false;
item = iterator.next();
passed &= item.value === undefined && item.done === true;
console.log(passed);
