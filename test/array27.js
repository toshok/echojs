// generator: babel-node

var arr = new Array ('hola', 'world', 'lots', 'of', 'fun');
console.log (arr.toString());

arr.fill ('1');
console.log (arr.toString());

arr.fill ('2', 2);
console.log (arr.toString());

arr.fill ('3', 2, 4);
console.log (arr.toString());

arr.fill ('4', 3, 100);
console.log (arr.toString());

arr.fill ('5', -3);
console.log (arr.toString());

arr.fill ('6', -3, -1);
console.log (arr.toString());

arr.fill ('7', 1, -2);
console.log (arr.toString());

arr.fill ('bug', -3, 0);
console.log (arr.toString());


