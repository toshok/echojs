console.log([1, 2, 3, 4].slice(1));
console.log([].slice.call([1, 2, 3, 4], 1));

console.log([].slice.call({ length: 3, 0: 1, 1: 2, 2: 3 }, 1));
