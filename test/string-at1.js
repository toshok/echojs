var s = "abc";
console.log(s.at(0));
console.log(s.at(2));
console.log(s.at(-1));
console.log(s.at(-3));

// out of range is undefined
console.log(s.at(3));
console.log(s.at(-4));
console.log(s.at(Infinity));
console.log(s.at(-Infinity));

// index goes through ToIntegerOrInfinity
console.log(s.at(1.5));
console.log(s.at(-0.5));
console.log(s.at(NaN));
console.log(s.at("2"));
console.log(s.at());

console.log("".at(0));

console.log("".at.length);
