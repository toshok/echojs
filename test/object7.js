if (typeof(console) == "object") var print = console.log;

var a = {};

Object.defineProperty (a, "c", { configurable: true, value: 10 });

var desc = Object.getOwnPropertyDescriptor(a, "c");
print (desc.value);
print (desc.configurable);
print (desc.writable);
print (desc.enumerable);
