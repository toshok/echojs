
var a = new String("hi there");

var toString = Object.prototype.toString;

if (typeof console !== "undefined") var print = console.log

print ("1: " + (Object.__proto__ === Function.__proto__))
print ("2: " + (Object.__proto__ === String.__proto__))
print ("3: " + (Object.__proto__ === RegExp.__proto__))
print ("4: " + (Object.__proto__ === Date.__proto__))
print ("5: " + (Object.prototype === Math.__proto__))
print ("6: " + (Object.prototype === JSON.__proto__))
