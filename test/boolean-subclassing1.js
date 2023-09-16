// generator: none

// "Boolean is subclassable" from kangax
function test() {
    class C extends Boolean {}
    var c = new C(true);
    return c instanceof Boolean && c == true;
}
console.log(test());
