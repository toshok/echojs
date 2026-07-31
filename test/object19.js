// `__proto__:` in an object literal is a prototype definition, not an
// own property (PropertyDefinitionEvaluation / B.3.1)

let o = { get x() { return 1; }, __proto__: { z: 9 } };
console.log(o.x, o.z);

let q = { __proto__: null, a: 1 };
console.log(q.a, typeof q.toString);

// non-object values are silently ignored
let r = { __proto__: 42, b: 2 };
console.log(r.b, typeof r.toString);

let s = { "__proto__": { w: 3 }, c: 4 };
console.log(s.c, s.w);
