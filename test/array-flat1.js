console.log([1, 2, [3, 4]].flat());
console.log([1, [2, [3, [4]]]].flat());
console.log([1, [2, [3, [4]]]].flat(2));
console.log([1, [2, [3, [4]]]].flat(Infinity));
console.log([1, [2, [3]]].flat(0));
console.log([1, [2, [3]]].flat(-1));
console.log([1, [2, [3]]].flat(NaN));
console.log([[[1]]].flat(1.9));
console.log([].flat());
console.log([[], [[]]].flat());

// holes are skipped
console.log([1, , 3].flat());
console.log([1, [2, , 4]].flat());

// only arrays are flattened
console.log([1, "ab", { length: 1, 0: "no" }].flat());

// the result is a new array
var orig = [1, [2]];
var flattened = orig.flat();
console.log(flattened == orig);
console.log(orig);

console.log([].flat.length);
