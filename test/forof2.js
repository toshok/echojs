
function foo() { }
foo.prototype[Symbol.iterator] = function() {
  var i = 0;
  return {
    next: function() {
      var d = i == 10;
      return { value: i ++, done: d };
    }
  };
};

var o = new foo();
for (var x of o) {
  console.log(x);
}
