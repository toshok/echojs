// generator: babel-node

// "yield *, strings" from kangax

var iterator = (function* generator() {
    yield* "56";
})();
var item = iterator.next();
var passed = item.value === "5" && item.done === false;
item = iterator.next();
passed &= item.value === "6" && item.done === false;
item = iterator.next();
passed &= item.value === undefined && item.done === true;
console.log(passed);
