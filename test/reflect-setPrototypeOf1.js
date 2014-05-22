
function test(l) {
    try { console.log(l()); } catch (e) { console.log(e); }
}

var a = {};
var b = {};

test( () => Reflect.getPrototypeOf(a) !== b);
Reflect.setPrototypeOf(a, b);
test( () => Reflect.getPrototypeOf(a) === b);
