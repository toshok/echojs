
var a = {}

toString = Object.prototype.toString;

if (typeof console !== "undefined") print = console.log
print (toString.call(a.prototype));
print (toString.call(a.__proto__));

print (a.__proto__ === Object.prototype);
print (a.__proto__ === Object.__proto__);
