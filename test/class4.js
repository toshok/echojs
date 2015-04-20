// methods aren't enumerable from kangax
class C {
  foo() {}
  static bar() {}
}
console.log( !C.prototype.propertyIsEnumerable("foo") && !C.propertyIsEnumerable("bar") );
