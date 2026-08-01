// async generators: protocol basics, awaits, delegation, throw/return
function delay(v, ms) { return new Promise(res => setTimeout(() => res(v), ms)); }

async function* counter(n) {
  for (let i = 0; i < n; i++) yield await delay(i, 1);
  return "done";
}

async function* letters() { yield "a"; yield "b"; return "lret"; }

async function* delegating() {
  const r = yield* letters();          // async delegate, value position
  yield "deleg:" + r;
  yield* [10, 20];                     // sync iterable delegate
  yield await delay("last", 1);
}

async function* catcher() {
  try { yield 1; } catch (e) { yield "caught:" + e; }
}

async function* cleaner() {
  try { yield "open"; yield "never"; } finally { console.log("cleanup"); }
}

class Feed {
  base = 100;
  async *ticks() { yield this.base + 1; yield await delay(this.base + 2, 1); }
}
const objLit = { async *m() { yield "objm"; } };

async function main() {
  for await (const v of counter(3)) console.log("c", v);
  for await (const v of delegating()) console.log("d", v);

  // manual protocol: queued nexts settle in order
  const it = letters();
  const [p1, p2, p3] = [it.next(), it.next(), it.next()];
  console.log((await p1).value, (await p2).value, (await p3).value, (await p3).done);

  // throw resumes at the suspended yield
  const tc = catcher();
  console.log((await tc.next()).value);
  console.log((await tc.throw("boom")).value);

  // return runs finallys and completes with the sent value
  const cl = cleaner();
  console.log((await cl.next()).value);
  const r = await cl.return("sent");
  console.log(r.value, r.done);

  for await (const v of new Feed().ticks()) console.log("t", v);
  for await (const v of objLit.m()) console.log("o", v);

  // an async generator is its own async iterator
  const self = letters();
  console.log(self[Symbol.asyncIterator]() === self);
}
main();
