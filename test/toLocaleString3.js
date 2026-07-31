// xfail: Number.prototype.toLocaleString lacks ICU's default maximumFractionDigits=3 rounding (node: 1.236, ejs: 1.2355)

var a = [1.2355, 1.2, "hi there", { a: 5 }];

console.log(a.toLocaleString());
