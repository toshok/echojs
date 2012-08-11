if (typeof console === "undefined") { console = { log: print }; }

function hello_outer (x) {
  function hello_inner () {
      console.log (x);
  }

  hello_inner();
}

hello_outer('hello world');
