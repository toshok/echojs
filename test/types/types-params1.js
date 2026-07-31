// numeric params, module-local call sites: the oracle sees every call,
// types the params {number}, and the bodies diamond
function hyp2(x, y) { return x * x + y * y; }
function scale(v, k) { return v / k; }
console.log(hyp2(3, 4));
console.log(scale(hyp2(6, 8), 4));
console.log(scale(1, 0)); // Infinity through a real fdiv fast path
