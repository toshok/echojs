if (typeof console === "undefined") { console = { log: print }; }

var obj = {
  str: "hello world"
};

var __hasProp = Object.prototype.hasOwnProperty;

console.log (__hasProp.call(obj, "str"));
