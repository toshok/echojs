const a = 123n;
const b = 10n ** 20n;
console.log(typeof a, a, b);
console.log(a + 1n, a * a, b / 3n, b % 7n, -a, ~a);
console.log(2n ** 64n);
console.log(a < 200n, a < 122, 123n == 123, 123n === 123n, 1n < 2, 2n > "1");
console.log(BigInt(42), BigInt("0x10"), BigInt.asUintN(8, 257n), BigInt.asIntN(8, 200n));
console.log((255n).toString(16), 0xffn, 0b101n, 0o17n, 1_000_000n);
console.log(5n & 3n, 5n | 3n, 5n ^ 3n, -5n & 3n, 1n << 100n, (1n << 100n) >> 99n);
let i = 5n; i++; ++i; i--; console.log(i);
console.log(Boolean(0n), Boolean(1n), 0n == false);
try { 1n + 1; } catch (e) { console.log("mix:", e.constructor.name); }
try { JSON.stringify(1n); } catch (e) { console.log("json:", e.constructor.name); }
console.log(BigInt.prototype.toString.call(7n), Object(7n) instanceof Object);
