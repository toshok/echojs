// xfail: Number.prototype.toLocaleString lacks ICU's default maximumFractionDigits=3 rounding (node: 1.236, ejs: 1.2355).  stale-baseline zombie flushed by runtime-P3

var a = [1.2355, 1.2, "hi there", { a: 5 }];

console.log(a.toLocaleString());
