// generator: none
// "Regexp.prototype.exec" from kangax
function test() {
        class R extends RegExp {}
        var r = new R("baz","g");
        return r.exec("foobarbaz")[0] === "baz" && r.lastIndex === 9;
}
console.log(test());
