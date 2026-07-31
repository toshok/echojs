let x = { b: 2, c: 3, d: 4 };
let { b, ...rest } = x;
console.log(b, JSON.stringify(rest));
let { ...all } = x;
console.log(JSON.stringify(all));
let key = "c";
let { [key]: cv, ...norest } = x;
console.log(cv, JSON.stringify(norest));
function f({ a, ...others }) { return JSON.stringify(others); }
console.log(f({ a: 1, z: 26, y: 25 }));
let arr2 = [{ p: 1, q: 2 }];
for (let { p, ...qs } of arr2) console.log(p, JSON.stringify(qs));
