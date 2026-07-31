function* h() {
    var local = { x: 12345, s: "before" };
    var arr = [1, 2, 3];
    yield 0;
    yield local.x + arr.length;
    yield local.s;
}
function churn(n) {
    var t = 0;
    for (var i = 0; i < n; i++) { var o = { p: i, q: [i, i] }; t += o.p; }
    return t;
}
var it = h();
console.log(it.next().value);
churn(8000);
console.log(it.next().value);
churn(8000);
console.log(it.next().value);
