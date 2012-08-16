if (typeof console === "undefined") { console = { log: print }; }

var i, j;

outer: for (j = 0; j < 10; j = j + 1) {
  for (i = 0; i < 10; i = i + 1) {
    console.log ("hello world");
    if (i == 5)
      break outer;
  }
}
