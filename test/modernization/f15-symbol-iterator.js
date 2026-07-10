let o = { *[Symbol.iterator]() { yield 1; yield 2; } }; console.log([...o].join(","));
