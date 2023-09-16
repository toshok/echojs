// xfail: we differ from node on property descriptor flags.  verify with spec

if (typeof console == "object") var print = console.log;

var a = {};
Object.defineProperty(a, "null", {
    get: function () {
        return 15;
    },
});

var desc = Object.getOwnPropertyDescriptor(a, "null");
print(desc.get);
print(desc.value);
print(desc.configurable);
print(desc.writable);
print(desc.enumerable);
print(a["null"]);
