// per-iteration environments: closures capturing let/const loop variables
// see their own iteration's binding (EIR loop envs; no DesugarLetLoopVars)

function forCapture() {
    let fns = [];
    for (let i = 0; i < 3; i++) fns.push(function () { return i; });
    return fns.map(function (g) { return g(); }).join(",");
}
function forOfCapture(xs) {
    let fns = [];
    for (let x of xs) fns.push(function () { return x; });
    return fns.map(function (g) { return g(); }).join(",");
}
function forInCapture(o) {
    let fns = [];
    for (let k in o) fns.push(function () { return k; });
    return fns.map(function (g) { return g(); }).sort().join(",");
}
function mixedCapture(base) {
    let fns = [];
    for (let i = 0; i < 2; i++) {
        for (let j = 0; j < 2; j++) fns.push(function () { return base + ":" + i + "" + j; });
    }
    return fns.map(function (g) { return g(); }).join(" ");
}
function continueCapture(xs) {
    let fns = [];
    for (let i = 0; i < xs.length; i++) {
        if (xs[i] < 0) continue;
        fns.push(function () { return xs[i]; });
    }
    return fns.map(function (g) { return g(); }).join(",");
}
function updateAfterCapture() {
    let fns = [];
    for (let i = 0; i < 3; i += 1) {
        fns.push(function (d) { i = i + d; return i; });
    }
    // each closure mutates its own iteration's binding
    return fns.map(function (g) { return g(10); }).join(",") + "/" + fns.map(function (g) { return g(0); }).join(",");
}
function constForInCapture(o) {
    let fns = [];
    for (const k in o) fns.push(function () { return k; });
    return fns.map(function (g) { return g(); }).sort().join(",");
}
console.log(forCapture());
console.log(forOfCapture(["a", "b", "c"]));
console.log(forInCapture({ p: 1, q: 2 }));
console.log(constForInCapture({ u: 1, v: 2 }));
console.log(mixedCapture("m"));
console.log(continueCapture([5, -1, 7]));
console.log(updateAfterCapture());
