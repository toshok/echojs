// "computed shorthand methods" from kangax
function test() {
  var x = 'y';
  return ({ [x](){ return 1 } }).y() === 1;
}

console.log(test());

