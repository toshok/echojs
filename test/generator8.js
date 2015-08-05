// xfail: generator support isn't 100%

// "%GeneratorPrototype%.return" from kangax

function * generator(){
  yield 5; yield 6;
};
var iterator = generator();
var item = iterator.next();
var passed = item.value === 5 && item.done === false;
item = iterator.return('quxquux');
passed    &= item.value === 'quxquux' && item.done === true;
item = iterator.next();
passed    &= item.value === undefined && item.done === true;
console.log(passed);

