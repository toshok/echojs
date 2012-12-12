
function foo(a) {
  switch (a) {
    case 0:
      console.log ("0 case");
      try {
        console.log ("before break");
        if (a == 0) break;
        console.log ("after break");
      }
      catch (e) { /* nada */ }
      break;
    case 1:
      console.log ("1 case");
      break;
  }
}

foo(0);
foo(1);
