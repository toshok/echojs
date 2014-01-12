if (typeof console !== "undefined") var print = console.log;
var a = { b: 1, c: 2, d: 3, e: 4, f: 5 };
print (JSON.stringify (a, Object.getOwnPropertyNames(a), 2));
print (JSON.stringify (a));
