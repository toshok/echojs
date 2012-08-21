if (typeof console === "undefined") { console = { log: print }; }

var array = [];
array.push (5);
console.log (array.length);

array.push (1,2,3,4);
console.log (array.length);

b = array.pop();
console.log (array.length);
console.log (b);
