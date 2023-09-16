// test array-with-holes length
var arr = [, ,];
console.log(arr.length);
for (var el of arr) {
    console.log(el);
}
