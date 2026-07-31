class S {
  static count = 3;
  static #priv = "sp";
  static tag = "t-" + S.count;
  static { S.fromBlock = S.count * 2; this.alsoThis = 1; }
  static readPriv() { return S.#priv; }
  static #inc() { return "si"; }
  static callInc() { return S.#inc(); }
}
console.log(S.count, S.tag, S.fromBlock, S.alsoThis, S.readPriv(), S.callInc());
class T { static a = 1; static { T.b = T.a + 1; } static c = T.b + 1; }
console.log(T.a, T.b, T.c);
