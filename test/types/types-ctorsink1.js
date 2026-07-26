// constructor-result sinking probe (docs/sinking-plan.md): the alloc
// kernel virtualizes behind the accessor-epoch check, and the epoch
// must retire it the moment anything intercept-capable lands on the
// prototype chain.  The interceptors are installed through
// Object.prototype — installing through Point.prototype would already
// decline the sink statically (the ctor's loads must all be callees),
// so this file exercises the RUNTIME half of the contract: a clean run
// first, then a mid-loop accessor install, then a mid-loop non-writable
// data install, each byte-compared against node.  (defineProperty, not
// accessor literals — the oracle can't normalize the latter.)
function Point(x, y) {
    this.x = x;
    this.y = y;
}

function run(n, flip, installer) {
    var s = 0;
    var i = 0;
    while (i < n) {
        if (i === flip) installer();
        var p = new Point(i, i + 1);
        s = s + p.x + p.y;
        i = i + 1;
    }
    return s;
}

function nothing() {}

function installAccessor() {
    Object.defineProperty(Object.prototype, "x", {
        configurable: true,
        set: function (v) {
            this.hx = v * 100;
        },
        get: function () {
            return this.hx + 7;
        },
    });
}

function installFrozenData() {
    Object.defineProperty(Object.prototype, "y", {
        configurable: true,
        value: 4242,
        writable: false,
    });
}

// clean epoch: the virtual arm runs the whole loop
console.log(run(1000, -1, nothing));
// accessor lands at i===5: constructions from there on are intercepted
// (this.x = v stores hx, p.x reads hx + 7)
console.log(run(1000, 5, installAccessor));
// still installed on later runs
console.log(run(10, -1, nothing));
// a non-writable data property also intercepts: this.y = v is silently
// swallowed and p.y reads the prototype's 4242
console.log(run(1000, 7, installFrozenData));
console.log(run(10, -1, nothing));
