if (typeof console === "undefined") { console = { log: print }; }

var a = 4 || 5;
var b = 4 && 5;

if (a === 4 && b === 5)
  console.log ("hello world");
else
  console.log ("goodbyte world");
