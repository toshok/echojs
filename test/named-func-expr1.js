// xfail: we don't add the name of a function expression to its inner scope
var g = function f() {
  console.log(typeof f);
};
g();
console.log(typeof f);
