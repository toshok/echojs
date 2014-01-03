
var Class = {
  create: function() {
    return function() {
      this.initialize.apply(this, arguments);
    };
  }
};

var Foo = Class.create();
Foo.prototype = {
  initialize: function (x, y, z) {
    console.log (x);
    console.log (y);
    console.log (z);
  }
};


var foo = new Foo(1, 2, 3);
