if (typeof console !== "undefined") var print = console.log

function tryInvoke(f) {
  try {
    f();
  }
  catch (e) { print ("f = null"); }
}

function foo(x) {
  f();

  if (x < 5) {
    function f() {
      print ("hello world");
    }
  }
  else {
    function f() {
      print ("hello world2");
    }
  }

  f();
}

foo(1);
foo(27);
