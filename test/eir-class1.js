// classes through the EIR pipeline (DesugarClasses runs pre-EIR; the
// class intrinsics lower via lib/eir/intrinsics.js)

function basics() {
    class P {
        constructor(x) { this.x = x; }
        val() { return this.x; }
        static tag() { return "P!"; }
    }
    let p = new P(7);
    return p.val() + "/" + P.tag();
}

function derived(v) {
    class A {
        constructor(x) { this.x = x; }
        describe() { return "A(" + this.x + ")"; }
    }
    class B extends A {
        constructor(x) { super(x + 1); this.v = v; }
        describe() { return "B[" + super.describe() + "," + this.v + "]"; }
    }
    let b = new B(10);
    return b.describe() + " " + (b instanceof A) + (b instanceof B);
}

function defaultCtor() {
    class A { constructor() { this.who = "A"; } hi() { return "hi " + this.who; } }
    class B extends A {}
    return new B().hi();
}

function accessors() {
    class T {
        constructor() { this._n = 1; }
        get n() { return this._n * 10; }
        set n(v) { this._n = v + 1; }
    }
    let t = new T();
    let before = t.n;
    t.n = 4;
    return before + "," + t.n;
}

function classExpr(k) {
    let C = class { constructor() { this.k = k; } };
    return new C().k;
}

function superSpread() {
    class A { constructor(a, b, c) { this.sum = a + b + c; } }
    class B extends A { constructor(xs) { super(...xs); } }
    return new B([1, 2, 3]).sum;
}

console.log(basics());
console.log(derived("z"));
console.log(defaultCtor());
console.log(accessors());
console.log(classExpr("kk"));
console.log(superSpread());
