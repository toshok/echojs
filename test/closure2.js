
var debug = function () { console.log ("hi"); }

function bar() {
  return function inner () {
    debug();
  }
}

bar()();
