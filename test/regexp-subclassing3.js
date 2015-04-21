// "RegExp.prototype.test" from kangax
function test() {
        class R extends RegExp {}
        var r = new R("baz");
        return r.test("foobarbaz");
}
console.log(test());
