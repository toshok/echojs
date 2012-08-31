
function tryInvoke(f) {
  try {
    f();
  }
  catch (e) { console.log ("f = null"); }
}

function foo(x) {
  f();

  if (x < 5) {
    function f() {
      console.log ("hello world");
    }
  }
  else {
    function f() {
      console.log ("hello world2");
    }
  }

  f();
}

foo(1);
foo(27);
