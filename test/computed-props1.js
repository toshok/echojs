var x = 'y';
console.log (({ [x]: 1 })['y'] === 1);

var o = {
 [Symbol.toStringTag]: "Hey"
};

console.log(Object.prototype.toString.call(o));
