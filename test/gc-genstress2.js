// values whose ONLY reference lives in a suspended generator's stack
// frames, across GCs forced from the main stack
function* h() {
    var local = { x: 12345, s: "before" };
    var arr = [1, 2, 3];
    yield 0;                       // suspend with local/arr live only here
    yield local.x + arr.length;    // use them after resumes+GCs
    yield local.s;
}
function churn(n) {
    var t = 0;
    for (var i = 0; i < n; i++) { var o = { p: i, q: [i, i] }; t += o.p; }
    return t;
}
var it = h();
console.log(it.next().value);
churn(400000);                     // force collections while h is suspended
console.log(it.next().value);
churn(400000);
console.log(it.next().value);
