// generator: babel-node
// "yield *, arrays" from kangax

var iterator = (function * generator() {
  yield * [5, 6];
}());
var item = iterator.next();
var passed = item.value === 5 && item.done === false;
item = iterator.next();
passed    &= item.value === 6 && item.done === false;
item = iterator.next();
passed    &= item.value === undefined && item.done === true;
console.log(passed);
