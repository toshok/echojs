function f({a = 1, b = 2} = {}) { return a + b; } console.log(f(), f({a: 10}));
