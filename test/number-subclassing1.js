// need to figure out babel error here
// generator: none
// xfail: subclassing builtins is broken

// "Number is subclassable" from kangax
function test() {
        class C extends Number {}
        var c = new C(6);
        return c instanceof Number
          && +c === 6;
}
console.log(test());
