class P { x = 1; y = this.x + 1; z; }
let p = new P();
console.log(p.x, p.y, p.z, Object.keys(p).join(","));
let kn = 0;
function key() { return "k" + kn++; }
class CK { [key()] = "a"; [key()] = "b"; }
let ck1 = new CK(), ck2 = new CK();
console.log(ck1.k0, ck1.k1, ck2.k0, kn);
class Base { constructor() { this.fromBase = this.probe ? this.probe() : "none"; } }
class D extends Base { probe() { return "m"; } df = 10; constructor() { super(); this.after = this.df + 1; } }
let d = new D();
console.log(d.fromBase, d.df, d.after);
let outer = "outer";
class Sh { f = outer; constructor(outer2) { this.o2 = outer2; } }
console.log(new Sh("arg").f, new Sh("arg").o2);
class SB { m() { return "sb-m"; } }
class SD extends SB { fld = super.m() + "!"; }
console.log(new SD().fld);
