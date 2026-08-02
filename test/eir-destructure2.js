// pattern (and member-expression) loop heads, catch-parameter patterns,
// nested spreads, debugger statements: all lower natively.

function forOfArrayPattern(ps) {
    let r = 0;
    for (let [a, b] of ps) r += a * b;
    return r;
}

function forOfObjectPattern(items) {
    let names = [];
    for (const { name, n } of items) names.push(`${name}:${n}`);
    return names.join(",");
}

function forOfPatternCapture(ps) {
    // body-scoped lets are per-iteration: each closure sees its own a/b
    let fns = [];
    for (let [a, b] of ps) fns.push(() => a + b);
    return fns.map((f) => f()).join(",");
}

function forOfMemberTarget(xs) {
    let o = { last: null, seen: [] };
    for (o.last of xs) o.seen.push(o.last);
    return `${o.seen.join("-")}|${o.last}`;
}

function forInPattern(obj) {
    let ks = [];
    for (const k in obj) ks.push(k);
    return ks.sort().join(",");
}

function forOfAssignmentPattern(ps) {
    let a, b;
    let sums = [];
    for ([a, b] of ps) sums.push(a + b);
    return sums.join(",");
}

function catchPattern(f) {
    try {
        f();
        return "no throw";
    } catch ({ message, code = 42 }) {
        return `${message}/${code}`;
    }
}

function nestedSpread(xs) {
    return [...[...xs, 5], 6];
}

function debuggerNoop(x) {
    debugger;
    return x + 1;
}

console.log(forOfArrayPattern([[1, 2], [3, 4]]));
console.log(forOfObjectPattern([{ name: "a", n: 1 }, { name: "b", n: 2 }]));
console.log(forOfPatternCapture([[1, 2], [30, 4]]));
console.log(forOfMemberTarget(["x", "y", "z"]));
console.log(forInPattern({ q: 1, r: 2 }));
console.log(forOfAssignmentPattern([[1, 1], [2, 3]]));
console.log(catchPattern(() => { throw new Error("boom"); }));
console.log(catchPattern(() => 0));
console.log(nestedSpread([1, 2]).join(" "));
console.log(debuggerNoop(9));
