
function Foo() {
}
Foo.prototype[Symbol.iterator] = function() {
  var i = 0;
  return { next: function() { return { value: i++, done: i == 10 } } };
};

var foo = new Foo();
console.log ([1, ...foo][5]);
