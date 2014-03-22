var arr = new Array ('hola', 'world', 'lots', 'of', 'fun');
console.log (arr);

arr.fill ('1');
console.log (arr);

arr.fill ('2', 2);
console.log (arr);

arr.fill ('3', 2, 4);
console.log (arr);

arr.fill ('4', 3, 100);
console.log (arr);

arr.fill ('5', -3);
console.log (arr);

arr.fill ('6', -3, -1);
console.log (arr);

arr.fill ('7', 1, -2);
console.log (arr);

arr.fill ('bug', -3, 0);
console.log (arr);


