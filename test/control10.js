if (typeof console === "undefined") { console = { log: print }; }

var j,i;
outer: for (j = 0; j < 5; j = j + 1) {
  for (i = 0; i < 5; i = i + 1) {
    console.log ("hello world");
    continue outer;
  }

}
