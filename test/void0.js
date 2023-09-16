function bar() {
    var _ref;
    function foo(n) {
        var _ref;

        return (n != null ? ((_ref = n.proto) != null ? _ref.foo : void 0) : void 0) != null;
    }

    function use_ref() {
        console.log(_ref);
    }
}
