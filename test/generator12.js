// "yield *, astral plane strings" from kangax

var iterator = (function * generator() {
  yield * 'u20BB7u20BB6;';
}());
var item = iterator.next();
var passed = item.value === 'u20BB7' && item.done === false;
item = iterator.next();
passed    &= item.value === 'u20BB6' && item.done === false;
item = iterator.next();
passed    &= item.value === undefined && item.done === true;
console.log(passed);

