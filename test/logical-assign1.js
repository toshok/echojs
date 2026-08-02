let x = 0; x ||= 5; console.log(x);
let y = 1; y &&= 7; console.log(y);
let z; z ??= "zz"; console.log(z);
let o = { p: 0, q: null };
o.p ||= 9; console.log(o.p);
o.q ??= "qq"; console.log(o.q);
let getCount = 0;
let base = { get obj() { getCount++; return o; } };
base.obj.p &&= 3; console.log(o.p, getCount);
let arr = [null]; arr[0] ??= "el"; console.log(arr[0]);
let calls = 0;
function rhs() { calls++; return "never"; }
let f0 = 0; f0 &&= rhs(); console.log(f0, calls);
let t1 = 1; t1 ||= rhs(); console.log(t1, calls);
let n1 = "set"; n1 ??= rhs(); console.log(n1, calls);
