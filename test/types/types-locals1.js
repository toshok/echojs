// pure numeric locals: every binary op below is diamond-eligible
var a = 3;
var b = 4;
var c = a * a + b * b;
var d = c / 5;
console.log(c);
console.log(d);
console.log(a - b);
console.log(a < b);
