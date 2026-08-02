// the arguments object is iterable (@@iterator = %ArrayProto_values%),
// and its specops must not ToNumber symbol keys.  the
// `super(...arguments)` shape is what tsc synthesizes for field-bearing
// subclasses without explicit constructors.

function spread() {
    return [...arguments].join(",");
}
console.log(spread(1, 2, 3));

function viaCall() {
    return Math.max(...arguments);
}
console.log(viaCall(4, 9, 2));

class A {
    constructor(x, y) {
        this.sum = x + y;
    }
}
class B extends A {
    constructor() {
        super(...arguments);
        this.tagged = true;
    }
}
let b = new B(20, 22);
console.log(b.sum, b.tagged);

// symbol-keyed reads on arguments delegate to the property map
function symprobe() {
    return typeof arguments[Symbol.iterator];
}
console.log(symprobe());

// index reads at and past argc are undefined, not garbage
function edge(a) {
    return [arguments[0], arguments[1], arguments[2]].join(",");
}
console.log(edge(5));
