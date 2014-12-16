class C {}
var c1 = C;
{
  class C {};
  var c2 = C;
}
console.log (C === c1);
