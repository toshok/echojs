// destructuring through the EIR pipeline (the first DesugarDestructuring
// run happens pre-EIR; %createIteratorWrapper lowers as a runtime call)

function objPattern(o) {
    let { a, b: renamed } = o;
    return a + "," + renamed;
}

function nestedPattern(o) {
    let { x: { y }, z } = o;
    return y + "," + z;
}

function arrayPattern(xs) {
    let [p, , q] = xs;
    return p + "," + q;
}

function arrayRest(xs) {
    let [head, ...tail] = xs;
    return head + "/" + tail.join("+");
}

function patternDefaults(o) {
    // AssignmentPattern in patterns: panicked the whole compiler before
    let { a = 10, b = 20 } = o;
    let [c = 30, d = 40] = o.arr;
    return [a, b, c, d].join(",");
}

function nestedDefault(o) {
    let { pos: { x = 1, y = 2 } = {} } = o;
    return x + "," + y;
}

function paramPattern({ a, b }, [c]) {
    return a + b + c;
}

function assignPosition(o) {
    let a, b;
    ({ a, b } = o);
    let c, d;
    [c, d] = [b, a];
    return a + "," + b + "/" + c + "," + d;
}

function swap(x, y) {
    [x, y] = [y, x];
    return x + "," + y;
}

console.log(objPattern({ a: 1, b: 2 }));
console.log(nestedPattern({ x: { y: "Y" }, z: "Z" }));
console.log(arrayPattern(["p", "skip", "q"]));
console.log(arrayRest([1, 2, 3, 4]));
console.log(patternDefaults({ b: 99, arr: [undefined, 44] }));
console.log(nestedDefault({}));
console.log(nestedDefault({ pos: { x: 7 } }));
console.log(paramPattern({ a: 1, b: 2 }, [3]));
console.log(assignPosition({ a: "A", b: "B" }));
console.log(swap("l", "r"));
