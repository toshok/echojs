// xfail: subclassing builtins is broken

// Array.of from kangax
class C extends Array {}
console.log(C.of(0) instanceof C);
