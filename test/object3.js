if (typeof console === "undefined") { console = { log: print }; }

var obj = {
  str: "hello world"
};

console.log (obj.hasOwnProperty("str"));
