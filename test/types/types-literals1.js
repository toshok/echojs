// literals mixed with typed vars: literals are oracle-unmapped glue but
// type directly in lowering (incl. the unary-minus parse of -2)
var x = 10;
console.log(x + 1);
console.log(x * 2);
console.log(x - -2);
console.log(x / 4);
console.log(x < 100);
