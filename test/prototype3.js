var a = new String("hi there");

var toString = Object.prototype.toString;

if (typeof console !== "undefined") var print = console.log;

print("1: " + toString.call(a.prototype));
print("2: " + toString.call(a.__proto__));

print("3: " + (a.__proto__ === Object.prototype));
print("4: " + (a.__proto__ === Object.__proto__));

print("5: " + (a.__proto__ === String.prototype));
print("6: " + (a.__proto__ === String.__proto__));

print("7: " + (a.prototype === null));
