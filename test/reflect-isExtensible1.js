
function test(l) {
    try { console.log(l()); } catch (e) { console.log(e); }
}


test( () => Reflect.isExtensible(null) );
test( () => Reflect.isExtensible(undefined) );

var a = {};

test( () => Reflect.isExtensible(a) );
Object.preventExtensions(a);
test( () => Reflect.isExtensible(a) );
