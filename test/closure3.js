// generator: babel-node

let a = 5;
function b() {
  console.log(a);
}

function c() {
  let d = 6;
  function e() { console.log(a); }
  function f() { console.log(d * a); }

  e();
  f();
}

b();
c();

