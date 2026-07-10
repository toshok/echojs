// arrow lexical `this` through the EIR pipeline: arrows read the owner
// function's captured this binding via the env chain

function methodArrow() {
    let o = {
        tag: "T",
        collect: function (xs) {
            return xs.map((x) => this.tag + ":" + x).join(",");
        },
    };
    return o.collect(["a", "b"]);
}

function nestedArrows() {
    let o = {
        n: 5,
        make: function () {
            return () => () => this.n * 2;
        },
    };
    return o.make()()();
}

function mixedCapture(prefix) {
    let o = {
        base: "B",
        run: function (k) {
            let local = k + 1;
            let f = () => prefix + this.base + local;
            return f();
        },
    };
    return o.run(1);
}

function detachedArrow() {
    let o = {
        who: "owner",
        getArrow: function () {
            return () => this.who;
        },
    };
    let f = o.getArrow();
    let other = { who: "other", f: f };
    // the arrow keeps its lexical this even called as a method of `other`
    return other.f();
}

function genMethodThis() {
    class Range {
        constructor(n) { this.n = n; }
        *items() { for (let i = 0; i < this.n; i++) yield i; }
    }
    let out = [];
    for (let v of new Range(3).items()) out.push(v);
    return out.join(",");
}

function ctorArrow() {
    class A { constructor(x) { this.x = x; } }
    class B extends A {
        constructor(x) {
            super(x);
            this.get = () => this.x + 1;
        }
    }
    return new B(41).get();
}

function arrowArguments() {
    let o = {
        m: function () {
            let f = () => arguments[0] + "/" + this.k;
            return f("ignored");
        },
        k: "K",
    };
    return o.m("outer");
}

console.log(methodArrow());
console.log(nestedArrows());
console.log(mixedCapture("p:"));
console.log(detachedArrow());
console.log(genMethodThis());
console.log(ctorArrow());
console.log(arrowArguments());
