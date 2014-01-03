var j = 0;
outer: while (j < 5) {
  j = j + 1;
  var i = 0;
  while (i < 10) {
    console.log ("hello world");
    continue outer;
  }

}
