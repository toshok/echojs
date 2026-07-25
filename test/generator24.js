// gc-plan P0: values whose ONLY references live in a SUSPENDED
// generator's stack frames must survive collections forced from the main
// stack.  Before the P0 fix the suspended-stack scan covered [stack, sp)
// — the dead region below the suspension point — missing every live frame.
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
