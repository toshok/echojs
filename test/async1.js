function delay(v, ms) { return new Promise(res => setTimeout(() => res(v), ms)); }
async function one() { return 1; }
async function two() { let a = await one(); let b = await delay(2, 10); return a + b; }
async function thrower() { throw new Error("async-throw"); }
async function catcher() {
  try { await thrower(); } catch (e) { return "caught:" + e.message; }
}
async function loops() {
  let sum = 0;
  for (let i = 0; i < 3; i++) sum += await delay(i, 1);
  return sum;
}
class Svc {
  base = 10;
  async fetch(x) { return this.base + await delay(x, 1); }
}
let arrowAsync = async (x) => (await delay(x, 1)) * 2;
const obj = { async m() { return "objm:" + await one(); } };
async function main() {
  console.log(await one());
  console.log(await two());
  console.log(await catcher());
  console.log(await loops());
  console.log(await new Svc().fetch(5));
  console.log(await arrowAsync(21));
  console.log(await obj.m());
  console.log(one() instanceof Promise);
  console.log((await Promise.all([one(), two(), "plain"])).join("|"));
}
main().then(() => console.log("main-done"));
console.log("sync-tail");
