// generator: none

// "Set is subclassable" from kangax
function test() {
        var obj = {};
        class S extends Set {}
        var set = new S();

        set.add(123);
        set.add(123);

        return set.has(123);
}
console.log(test());
