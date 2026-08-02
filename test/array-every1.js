// every/some coerce the callback result with ToBoolean: any falsy
// result fails the predicate, not just literal false
console.log([null, 1].every(function (x) { return x; }));
console.log([1, 2].every(function (x) { return x; }));
console.log([0, 1].every(function (x) { return x; }));
console.log(["", "a"].every(function (x) { return x; }));
console.log([undefined].every(function (x) { return x; }));
console.log([].every(function (x) { return x; }));
console.log([1, "a", {}].every(function (x) { return x; }));
console.log([0, null].some(function (x) { return x; }));
console.log([0, 3].some(function (x) { return x; }));
