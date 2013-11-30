var m = new Map();
m.set ("__proto__", 5);
console.log (m.has ("__proto__"));
console.log (m.get ("__proto__"));
console.log (Object.prototype.toString.call (m.__proto__));
