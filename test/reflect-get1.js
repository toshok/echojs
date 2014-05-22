
var o = {a: 5};
var fooReceiver = { foo : 10 };
Object.defineProperty (o, "b", { get: function () { console.log (this.foo); return 5; } });

console.log(Reflect.get(o, "a"));
console.log(Reflect.get(o, "b", fooReceiver));
