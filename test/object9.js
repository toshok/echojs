if (typeof(console) == "object") print = console.log;

var a = {};
Object.defineProperty (a, "null", {get: function() { return 15; } });

var desc = Object.getOwnPropertyDescriptor(a, "null");
print (desc.get);
print (desc.value);
print (desc.configurable);
print (desc.writable);
print (desc.enumerable);
print (a['null']);
