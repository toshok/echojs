// BigInt: division/modulo signs, pow edges, big values, loose equality
console.log(7n / 2n, -7n / 2n, 7n / -2n, -7n % 3n, 7n % -3n);
console.log(0n ** 0n, (-2n) ** 3n, (-1n) ** 1000000000000n);
try { 2n ** -1n; } catch (e) { console.log("negexp:", e.constructor.name); }
try { 1n / 0n; } catch (e) { console.log("div0:", e.constructor.name); }
try { 1n % 0n; } catch (e) { console.log("mod0:", e.constructor.name); }
const big = 123456789012345678901234567890n;
console.log(big * big);
console.log(big.toString(36), big.toString(2).length);
console.log(BigInt("  42  "), BigInt(""), BigInt(true), BigInt(false));
try { BigInt("42.5"); } catch (e) { console.log("frac-str:", e.constructor.name); }
try { BigInt(1.5); } catch (e) { console.log("frac-num:", e.constructor.name); }
try { BigInt(Infinity); } catch (e) { console.log("inf:", e.constructor.name); }
console.log(9007199254740993n == 9007199254740993, 9007199254740993n == 9007199254740992);
console.log(1n == "1", 1n == "0x1", 0n == "", 1n != "2");
console.log(10n > 9.5, 10n < 10.5, -0.5 < 0n, 0n <= -0);
console.log([3n, 1n, 2n].sort((a, b) => (a < b ? -1 : a > b ? 1 : 0)).join(","));
const m = new Map([[1n, "one"]]);
console.log(m.get(1n));
console.log(String(42n), `${42n}`, 42n + "!");
console.log(BigInt.asIntN(0, 5n), BigInt.asUintN(0, 5n), BigInt.asIntN(64, -1n));
console.log(Number(123n), Number(-(2n ** 64n)));
