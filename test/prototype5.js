
function A() { }
var a = new A();

var toString = Object.prototype.toString;

if (typeof console !== "undefined") var print = console.log

print ("1: " + (a.prototype === undefined));
print ("2: " + (toString.call(a.__proto__)));

print ("3: " + (a.__proto__ === A.prototype));
