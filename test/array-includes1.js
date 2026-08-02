console.log([1, 2, 3].includes(2));
console.log([1, 2, 3].includes(4));
console.log([1, 2, 3].includes("2"));

// SameValueZero: NaN is found, +0 and -0 are equal
console.log([1, NaN, 3].includes(NaN));
console.log([1, 2, 3].indexOf(NaN));
console.log([0].includes(-0));
console.log([-0].includes(0));

// fromIndex, negative counts from the end, clamped
console.log([1, 2, 3].includes(1, 1));
console.log([1, 2, 3].includes(3, -1));
console.log([1, 2, 3].includes(1, -1));
console.log([1, 2, 3].includes(1, -100));
console.log([1, 2, 3].includes(1, 3));
console.log([1, 2, 3].includes(2, Infinity));
console.log([1, 2, 3].includes(2, -Infinity));

// holes read as undefined
console.log([, 1].includes(undefined));
console.log([].includes(undefined));

console.log(["a", "b"].includes("b"));
console.log([null].includes(null));
console.log([undefined].includes(null));

console.log([].includes.length);
