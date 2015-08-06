// generator: babel-node

function test(l) {
    try { console.log(l()); } catch (e) { console.log(e.constructor.name); }
}


test( () => Reflect.isExtensible(null) );
test( () => Reflect.isExtensible(undefined) );

var a = {};

test( () => Reflect.isExtensible(a) );
Object.preventExtensions(a);
test( () => Reflect.isExtensible(a) );
