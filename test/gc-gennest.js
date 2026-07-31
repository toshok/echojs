// nested active generators: A's body drives B while both hold stack-only refs
function* inner(base) {
    var box = { v: base * 10, tag: "in" + base };
    yield box.v;
    yield box.tag;
}
function* outer() {
    var mine = { w: 7, s: [1, 2, 3] };
    var it = inner(3);
    yield it.next().value;   // B active inside A
    yield it.next().value;
    yield mine.w + mine.s.length;
}
function churn(n) {
    var t = 0;
    for (var i = 0; i < n; i++) { var o = { p: i }; t += o.p % 3; }
    return t;
}
var it = outer();
console.log(it.next().value);
churn(6000);
console.log(it.next().value);
churn(6000);
console.log(it.next().value);
