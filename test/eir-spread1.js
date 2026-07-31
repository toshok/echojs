// spread calls and spread array literals through the EIR pipeline
// (DesugarSpread runs pre-EIR; %arrayFromSpread lowers to
// array_from_spread)

function join3(a, b, c) {
    return a + "," + b + "," + c;
}

function callSpread(xs) {
    return join3(1, ...xs);
}

function arraySpread(xs, ys) {
    return [0, ...xs, 9, ...ys];
}

function methodSpread(xs) {
    let o = {
        base: "b",
        m: function (x, y) {
            return this.base + ":" + x + ":" + y;
        },
    };
    return o.m(...xs);
}

// a non-spread array-literal-with-spread argument next to a spread arg:
// the literal argument must survive DesugarSpread's %arrayFromSpread
// flattening
function mixedArgs(xs, ys) {
    return join3(...xs, [1, ...ys].join("+"));
}

function nestedSpread(xs) {
    return [...[...xs, 5], 6];
}

console.log(callSpread([2, 3]));
console.log(arraySpread([1, 2], [3]).join(" "));
console.log(methodSpread(["x", "y"]));
console.log(mixedArgs([7, 8], [2, 3]));
console.log(nestedSpread([4]).join(""));
console.log(join3(...["t"], ...[], ...["u", "v"]));
