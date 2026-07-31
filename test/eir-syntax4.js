function argsLen() {
    return arguments.length;
}

function argsSum() {
    let s = 0;
    for (var i = 0; i < arguments.length; i++) s += arguments[i];
    return s;
}

function argsArrow() {
    let g = () => arguments.length + ":" + arguments[0];
    return g();
}

function objPat(o) {
    // NOTE: no pattern defaults here — the legacy pipeline's
    // DesugarDestructuring panics on AssignmentPattern (EIR supports
    // them, but suite tests must pass both pipelines)
    let { a, b: c } = o;
    let d = o.d === undefined ? 9 : o.d;
    return `${a}/${c}/${d}`;
}

function delMember(o) {
    delete o.x;
    delete o["y"];
    return JSON.stringify(o);
}

function afterInfinite(n) {
    while (true) {
        if (n > 2) break;
        n++;
    }
    var node = n * 10;
    return node;
}

console.log(argsLen(), argsLen(1, 2, 3));
console.log(argsSum(1, 2, 3, 4));
console.log(argsArrow("x", "y"));
console.log(objPat({ a: 1, b: 2 }), "|", objPat({ a: 1, b: 2, d: 3 }));
console.log(delMember({ x: 1, y: 2, z: 3 }));
console.log(afterInfinite(0));
