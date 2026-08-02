let a = { b: { c: 42 }, f() { return this.b; }, n: null };
console.log(a?.b.c);
console.log(a.n?.c);
console.log(a.missing?.x.y.z);
console.log(a?.b?.c);
let nul = null;
console.log(nul?.x);
console.log(nul?.x.y());
console.log(nul?.[0]);
console.log(a.f?.().c);
console.log(a.nof?.());
let key = "c";
console.log(a.b?.[key]);
console.log(delete a.n?.x, delete nul?.x);
let fns = { g() { return this === fns ? "recv-ok" : "recv-BAD"; } };
console.log(fns.g?.());
let calls = 0;
function effect() { calls++; return a; }
effect()?.b; console.log(calls);
nul?.deep(effect()); console.log(calls); // arg not evaluated
class Base2 { m() { return "base-m"; } get g() { return { v: 10 }; } }
class Sub2 extends Base2 {
  m() { return super.m?.() + "!"; }
  n() { return super.missing?.(); }
  o() { return super.g?.v; }
}
let s = new Sub2();
console.log(s.m(), s.n(), s.o());
