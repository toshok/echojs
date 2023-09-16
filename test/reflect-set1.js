// generator: babel-node

var o = {};
var fooReceiver = { foo: "hello" };
Object.defineProperty(o, "b", {
    set: function (v) {
        console.log(this.foo);
        console.log(v);
    },
});

function test(l) {
    try {
        console.log(l());
    } catch (e) {
        console.log(e);
    }
}

test(() => Reflect.set(o, "a", 5));
test(() => o.a);
test(() => Reflect.set(o, "b", 10, fooReceiver));
