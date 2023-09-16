// generator: babel-node
// "Map is subclassable" from kangax
function test() {
    var key = {};
    class M extends Map {}
    var map = new M();

    map.set(key, 123);

    return map.has(key) && map.get(key) === 123;
}
console.log(test());
