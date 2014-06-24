
var arr = [ 1, 4, 6 , "hallo", "there" ];
var s = new Set (arr);

for (let a of arr)
    console.log (s.has (a));

console.log (s.has ("idontexist"));
console.log (s.has (3.1416));

