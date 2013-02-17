function f () {
  throw "hello world"
}

try {
  f();
}
catch (e) {
  console.log (e);
}

