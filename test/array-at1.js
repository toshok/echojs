var a = [1, 2, 3];
console.log(a.at(0));
console.log(a.at(2));
console.log(a.at(-1));
console.log(a.at(-3));

// out of range is undefined
console.log(a.at(3));
console.log(a.at(-4));
console.log(a.at(Infinity));
console.log(a.at(-Infinity));

// index goes through ToIntegerOrInfinity
console.log(a.at(1.5));
console.log(a.at(-0.5));
console.log(a.at(NaN));
console.log(a.at("2"));
console.log(a.at());

console.log([].at(0));

// generic over array-likes
console.log([].at.call({ length: 2, 0: "x", 1: "y" }, -1));

console.log([].at.length);
