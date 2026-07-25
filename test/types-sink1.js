function Point(x, y) { this.x = x; this.y = y; }
function alloc(n) {
    var s = 0; var i = 0;
    while (i < n) {
        var p = new Point(i, i + 1);
        s = s + p.x + p.y;
        i = i + 1;
    }
    return s;
}
var o = { a: 1, b: 2 };
console.log(alloc(200000) + o.a + o.b);
