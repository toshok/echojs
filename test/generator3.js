// generator: babel-node

// "correct this binding" from kangax

function * generator(){
  yield this.x; yield this.y;
};
var iterator = { g: generator, x: 5, y: 6 }.g();
var item = iterator.next();
var passed = item.value === 5 && item.done === false;
item = iterator.next();
passed    &= item.value === 6 && item.done === false;
item = iterator.next();
passed    &= item.value === undefined && item.done === true;
console.log(passed);

