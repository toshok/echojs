let a = [1, 2, 3, 4, 5];
console.log(a.slice(0, -1).join(","));
console.log(a.slice(-2).join(","));
console.log(a.slice(-4, -1).join(","));
console.log(a.slice(1, -10).length);
console.log(a.slice(0, undefined).join(","));
console.log(a.slice(-100).join(","));
