function test() {
    var a = 1, b = {};
    b[Symbol.iterator] = function() {
      return {
        next: function() {
          return { value: "foo", done: false };
        }
      };
    };

    for (var c of b) { console.log(c); return c === 'foo'; } return false;
}

console.log(test());
