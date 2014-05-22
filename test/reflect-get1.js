
var o = {a: 5};
var fooReceiver = { foo : 10 };
Object.defineProperty (o, "b", { get: function () { console.log (this.foo); return 5; } });

function test(l) {
    try { console.log(l()); } catch (e) { console.log(e); }
}

test( () => Reflect.get(o, "a") );   // 5
test( () => Reflect.get(o, "b", fooReceiver) ); // 10\n5

test( () => Reflect.get(null, "a") );  // TypeError
test( () => Reflect.get(undefined, "a") ); // TypeError
test( () => Reflect.get(o, "") );  // undefined
test( () => Reflect.get(o) ); // undefined
test( () => Reflect.get(o, null) ); // undefined
