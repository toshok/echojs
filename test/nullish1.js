console.log(null ?? "a");
console.log(undefined ?? "b");
console.log(0 ?? "no");
console.log("" ?? "no");
console.log(false ?? "no");
console.log(NaN ?? "no");
let count = 0;
function rhs() { count++; return 42; }
console.log(7 ?? rhs(), count);
console.log(null ?? rhs(), count);
console.log((null ?? undefined) ?? "chain");
