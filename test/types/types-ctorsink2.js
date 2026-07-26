// constructor-result sinking, the pure-win shape (docs/sinking-plan.md):
// a monomorphic alloc kernel with no interference anywhere — the
// canonical reduction is an allocation-free loop.  Also exercises the
// declines around it: a site whose result escapes keeps its construct,
// and a ctor whose prototype is touched anywhere declines wholesale.
function Point(x, y) {
    this.x = x;
    this.y = y;
}

function alloc(n) {
    var s = 0;
    var i = 0;
    while (i < n) {
        var p = new Point(i, i + 1);
        s = s + p.x + p.y;
        i = i + 1;
    }
    return s;
}

// Escaper's result flows into a call: that site must keep its construct
function sink2_keep(p) {
    return p.x;
}
function escaper(n) {
    var s = 0;
    var i = 0;
    while (i < n) {
        s = s + sink2_keep(new Point(i, i));
        i = i + 1;
    }
    return s;
}

// Touched's prototype carries a method: the load discipline declines
// every Touched construct (a swapped or decorated prototype is exactly
// what the static screen exists for), and the method keeps working
function Touched(x, y) {
    this.x = x;
    this.y = y;
}
Touched.prototype.sum = function () {
    return this.x + this.y;
};
function methods(n) {
    var s = 0;
    var i = 0;
    while (i < n) {
        s = s + new Touched(i, i + 1).sum();
        i = i + 1;
    }
    return s;
}

console.log(alloc(100000));
console.log(escaper(1000));
console.log(methods(1000));
