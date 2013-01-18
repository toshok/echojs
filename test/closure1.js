
var debug = function () { console.log ("hi"); }

function bar() {
  return debug;
}

bar()();
