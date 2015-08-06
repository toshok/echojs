// generator: babel-node

// implicit strict mode from kangax
class C {
  static method() { return this === undefined; }
}
console.log((0,C.method)());
