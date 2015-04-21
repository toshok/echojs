// "basic functionality" from kangax
function test() {
        class R extends RegExp {}
        var r = new R("baz","g");
        return r.global && r.source === "baz";
}
console.log(test());
