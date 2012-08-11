if (typeof console === "undefined") { console = { log: print }; }

function hello_outer (x) {
  return function () {
      console.log (x);
  };
}

var hello_inner = hello_outer('hello world');
hello_inner();
