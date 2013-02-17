function f() {}

if (typeof console !== "undefined") print = console.log

print (typeof f); // "function"
print (typeof f.prototype); // "object""
print (typeof Function.prototype); // "function"
print (f); // "[Function: f]"
print (f.toString()); // "function f() { }""
print (Function.prototype.toString.call(f)); // "function f() { }""
print (Object.prototype.toString.call(f)); // "[object Function]"
print (f.prototype === Function); // false
print (f.prototype === Function.prototype);  // false
print (f.prototype.prototype === Function.prototype);  // false
print (f.prototype.prototype === undefined); // true
print (Function.prototype.isPrototypeOf(f)); // true
print (Function.prototype.isPrototypeOf(f.prototype)); // false

print (f.prototype === Object.prototype); // false
print (Object.prototype.isPrototypeOf(f)); // true
print (Object.prototype.isPrototypeOf(f.prototype)); // true
print (Object.isPrototypeOf(f.prototype, Object.prototype)); // false
print (f.prototype.toString === Function.prototype.toString); // false
print (f.prototype.toString === Object.prototype.toString); // true
print (Object.prototype.toString === Function.prototype.toString); // false
print (f.toString === Function.prototype.toString); // true
print (f.toString.prototype === Function.prototype); // false
print (f.toString.prototype === undefined); // true
print (Object.prototype.hasOwnProperty.call(f, "toString")); // false
print (Object.prototype.hasOwnProperty.call(Function.prototype, "toString")); // true
print (Object.prototype.hasOwnProperty.call(Object.prototype, "toString")); // true
