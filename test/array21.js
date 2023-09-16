var a = [1, 2, 3, 4, 5];
Object.defineProperty(a, 9, { value: 6, configurable: true, writable: true, enumerable: true });
console.log(a);
console.log(a.length);
