// generator: babel-node

function test() {
    var a = 0, b = {};
    b[Symbol.iterator] = function() {
      return {
        next: function() {
          return { 
            done: a === 1,
            value: a++ 
          };
        }
      };
    };
    for (var c of b) { return c === 0; } return false;
}

console.log(test());
