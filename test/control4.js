if (typeof console === "undefined") { console = { log: print }; }

var i = 0;
while (i < 10) {
  console.log ("hello world");
  i = i + 1;
  if (i == 5)
    break;
}
