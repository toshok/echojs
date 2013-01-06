
function a() { }

toString = Object.prototype.toString;

if (typeof console !== "undefined") print = console.log

print ("1: " + (toString.call(a.prototype)));
print ("2: " + (toString.call(a.__proto__)));

print ("3: " + (a.__proto__ === Object.prototype));
print ("4: " + (a.__proto__ === Object.__proto__));

print ("5: " + (a.prototype.constructor === a));

print ("6: " + (a.__proto__ === Function.prototype));
print ("7: " + (a.__proto__ === Function.__proto__));
