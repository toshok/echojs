
var a = 0;
try {
  throw 5;
}
catch (e) {
  a = e;
}
console.log (a);
