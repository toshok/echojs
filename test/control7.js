if (typeof console === "undefined") { console = { log: print }; }

i = 0;
while (i < 5) {
  console.log ("hello world");
  i = i + 1;
  continue;
  console.log ("not reached");
}
