class Counter {
  #n = 0;
  #limit;
  constructor(limit) { this.#limit = limit; }
  inc() { if (this.#n < this.#limit) this.#n++; return this.#n; }
  get value() { return this.#n; }
  static has(o) { return #n in o; }
}
let c = new Counter(2);
console.log(c.inc(), c.inc(), c.inc(), c.value);
console.log(Counter.has(c), Counter.has({}), Object.keys(c).length);
try { Counter.prototype.inc.call({}); } catch (e) { console.log(e.constructor.name); }
class PM {
  #secret() { return "s:" + this.#val; }
  get #val() { return 42; }
  set #val(v) { this.pub = v; }
  run() { return this.#secret(); }
  setIt(v) { this.#val = v; return this.pub; }
  hasM(o) { return #secret in o; }
}
let pm = new PM();
console.log(pm.run(), pm.setIt(7), pm.hasM(pm), pm.hasM({}));
try { PM.prototype.run.call({}); } catch (e) { console.log("brand:" + e.constructor.name); }
class U { #v = 5; bump() { this.#v += 2; return this.#v; } post() { return this.#v++; } pre() { return ++this.#v; } }
let u = new U();
console.log(u.bump(), u.post(), u.pre());
class L { #a = null; fill(v) { this.#a ??= v; return this.#a; } }
let l = new L();
console.log(l.fill("x"), l.fill("y"));
class OC { #f = 9; read(o) { return o?.#f; } }
let oc = new OC();
console.log(oc.read(oc), oc.read(null));
