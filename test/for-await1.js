function delay(v, ms) { return new Promise(res => setTimeout(() => res(v), ms)); }
async function forAwaitSync() {
  let acc = [];
  for await (const v of [delay("a", 5), "b", delay("c", 1)]) acc.push(v);
  return acc.join("");
}
function ticker(n) {
  let i = 0;
  return {
    [Symbol.asyncIterator]() {
      return { next() { return i < n ? Promise.resolve({ value: i++, done: false }) : Promise.resolve({ value: undefined, done: true }); } };
    }
  };
}
async function collect() {
  const out = [];
  for await (const v of ticker(4)) out.push(v);
  return out.join("-");
}
async function withBreak() {
  let seen = [];
  for await (const v of [1, 2, 3, 4]) { if (v === 3) break; seen.push(v); }
  return seen.join(",");
}
async function main() {
  console.log(await forAwaitSync());
  console.log(await collect());
  console.log(await withBreak());
}
main().then(() => console.log("done"));
