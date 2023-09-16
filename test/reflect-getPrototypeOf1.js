// generator: none

function test(l) {
    try {
        console.log(l());
    } catch (e) {
        console.log(e.constructor.name);
    }
}

test(() => Reflect.getPrototypeOf(null));
test(() => Reflect.getPrototypeOf(undefined));

test(() => Reflect.getPrototypeOf({}) === Reflect.getPrototypeOf({}));
test(() => Reflect.getPrototypeOf({}) === Reflect.getPrototypeOf([]));
test(() => Reflect.getPrototypeOf(1) === Reflect.getPrototypeOf(2));
test(() => Reflect.getPrototypeOf(test));
