// xfail: XXX

// new ES6 math functions
console.log(Math.hypot(1,1))
console.log(Math.hypot(-1,-1))
console.log(Math.hypot(NaN,NaN))

console.log(Math.imul());
console.log(Math.imul(5));
console.log(Math.imul(5, 5));
console.log(Math.imul(5.2, 2.5));

console.log(Math.sign(-Infinity));
console.log(Math.sign(-1.1));
console.log(Math.sign(-0));
console.log(Math.sign(+0));
console.log(Math.sign(5));
console.log(Math.sign(Infinity));
console.log(Math.sign(NaN));

console.log(Math.log10(0));
console.log(Math.log10(10));
console.log(Math.log10(1000));
console.log(Math.log10(Infinity));
console.log(Math.log10(-1000));
console.log(Math.log10(NaN));

console.log(Math.log2(0));
console.log(Math.log2(64));
console.log(Math.log2(256));
console.log(Math.log2(Infinity));
console.log(Math.log2(-1024));
console.log(Math.log2(NaN));

console.log(Math.log1p(Math.E-1));

console.log(Math.expm1(-1));
console.log(Math.expm1(0));
console.log(Math.expm1(1));

console.log(Math.cosh(-1));
console.log(Math.cosh(0));
console.log(Math.cosh(1));

console.log(Math.sinh(-1));
console.log(Math.sinh(0));
console.log(Math.sinh(1));

console.log(Math.tanh(0));
console.log(Math.tanh(Infinity));
console.log(Math.tanh(1));

console.log(Math.acosh(-1));
console.log(Math.acosh(0));
console.log(Math.acosh(0.5));
console.log(Math.acosh(1));
console.log(Math.acosh(2));

console.log(Math.asinh(1));
console.log(Math.asinh(0));

console.log(Math.atanh(-2));
console.log(Math.atanh(-1));
console.log(Math.atanh(0));
console.log(Math.atanh(0.5));
console.log(Math.atanh(1));
console.log(Math.atanh(2));

console.log(Math.trunc(13.37));   // 13
console.log(Math.trunc(42.84));   // 42
console.log(Math.trunc(0.123));   //  0
console.log(Math.trunc(-0.123));  // -0
console.log(Math.trunc("-1.123"));// -1
console.log(Math.trunc(NaN));     // NaN
console.log(Math.trunc("foo"));   // NaN
console.log(Math.trunc());        // NaN

//console.log(Math.fround(0));     // 0
//console.log(Math.fround(1));     // 1
//console.log(Math.fround(1.337)); // 1.3370000123977661
//console.log(Math.fround(1.5));   // 1.5
//console.log(Math.fround(NaN));   // NaN

console.log(Math.cbrt(-1)); // -1
console.log(Math.cbrt(0));  // 0
console.log(Math.cbrt(1));  // 1
console.log(Math.cbrt(2));  // 1.2599210498948734

console.log(Math.clz32(0x111110));
console.log(Math.clz32(0));
