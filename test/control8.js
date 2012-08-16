if (typeof console === "undefined") { console = { log: print }; }

var i;
for (i = 0; i < 10; i = i + 1) {
  console.log ("hello world");
  if (i == 5)
    break;
}
