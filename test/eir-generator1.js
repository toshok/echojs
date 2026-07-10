// generators through the EIR pipeline (DesugarGeneratorFunctions runs
// pre-EIR; coroutine-style — %makeGenerator/%generatorYield lower as
// runtime calls)

function collect(g) {
    let out = [];
    for (let v of g) out.push(v);
    return out.join(",");
}

function basic() {
    function* seq() { yield 1; yield 2; yield 3; }
    return collect(seq());
}

function loopYield(n) {
    function* upto() { for (let i = 0; i < n; i++) yield i * 10; }
    return collect(upto());
}

function delegate() {
    function* inner() { yield "b"; yield "c"; }
    function* outer() { yield "a"; yield* inner(); yield "d"; }
    return collect(outer());
}

function sentValues() {
    function* echoing() {
        let got = yield "first";
        let got2 = yield "got:" + got;
        yield "got2:" + got2;
    }
    let g = echoing();
    let a = g.next().value;
    let b = g.next("one").value;
    let c = g.next("two").value;
    return a + "/" + b + "/" + c;
}

function doneProtocol() {
    function* two() { yield 1; yield 2; }
    let g = two();
    g.next(); g.next();
    let r = g.next();
    return r.done + "," + r.value;
}

function genMethod() {
    class Range {
        constructor(n) { this.n = n; }
        *items() { for (let i = 0; i < this.n; i++) yield i; }
    }
    // the desugared body is an arrow touching `this` -- exercises the
    // class + generator pre-EIR combination even when it falls back
    return collect(new Range(3).items());
}

console.log(basic());
console.log(loopYield(4));
console.log(delegate());
console.log(sentValues());
console.log(doneProtocol());
console.log(genMethod());
