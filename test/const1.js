// xfail: we permit assigning to const bindings

function f() {
  const i = 5;
  i = 10;
  return i;
}

console.log(f());
