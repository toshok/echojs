// yield* delegation: value position, sent-value forwarding, return value

function* inner() {
  const a = yield "i1";
  const b = yield "i2:" + a;
  return "iret:" + b;
}

function* outer() {
  const r = yield* inner();
  yield "after:" + r;
}

const g = outer();
console.log(g.next().value);        // i1
console.log(g.next("s1").value);    // i2:s1
console.log(g.next("s2").value);    // after:iret:s2
console.log(g.next().done);         // true

// delegate to a plain iterable, in expression position
function* arr() {
  const r = yield* [1, 2, 3];
  yield "arr-ret:" + r;              // arrays' return value is undefined
}
console.log([...arr()].join(","));

// nested delegation
function* a() { yield 1; return "ra"; }
function* bgen() { const r = yield* a(); yield "b:" + r; return "rb"; }
function* c() { const r = yield* bgen(); yield "c:" + r; }
console.log([...c()].join(","));

// gen.return through a delegate closes the inner iterator
let closed = false;
function makeIter() {
  let i = 0;
  return {
    [Symbol.iterator]() { return this; },
    next() { return { value: i++, done: false }; },
    return() { closed = true; return { value: undefined, done: true }; },
  };
}
function* d() { yield* makeIter(); }
const gd = d();
console.log(gd.next().value);       // 0
gd.return(99);
console.log("closed:", closed);     // true
console.log(gd.next().done);        // true
