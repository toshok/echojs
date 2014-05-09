
function foo(a, b, c) {
  console.log(a);
  console.log(b);
  console.log(c);
}

foo(1,...[2,3]);
// these two fail due to esprima parse errors. need to check the spec
//foo(...[1,2], 3);
//foo(...[1],...[2],...[3]);

