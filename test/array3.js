if (typeof console === "undefined") { console = { log: print }; }

var arr = ["hello world"];
arr.anotherprop = "hello world";
if (arr.length === 1)
  console.log (arr.anotherprop);
