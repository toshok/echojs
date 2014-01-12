if (typeof(console) == "object") var print = console.log;

var a = {b : 15};

var desc = Object.getOwnPropertyDescriptor(a, "b");
print (desc.value);
print (desc.configurable);
print (desc.writable);
print (desc.enumerable);
