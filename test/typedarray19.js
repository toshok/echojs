
// int8
let byteArr = Int8Array.of(1, 127, 66, 48);
console.log(byteArr);

byteArr.set([]);
console.log(byteArr);

byteArr.set([ 13 ]);
console.log(byteArr);

byteArr.set([ 21, 39, 43, 66 ]);
console.log(byteArr);

byteArr.set([ 68, 69 ], 1);
console.log(byteArr);

byteArr.set([ '', '5', undefined ]);
console.log(byteArr);

console.log();

// int32
let arr = Int32Array.of(1, 258, 1025, 2057);
console.log(arr);

arr.set([]);
console.log(arr);

arr.set([ 314 ]);
console.log(arr);

arr.set([ 1024, 2056, 3069, 4044 ]);
console.log(arr);

arr.set([ 666, 69 ], 1);
console.log(arr);

arr.set([ '', '2099' ]);
console.log(arr);

console.log();

// float64
let floatArr = Float64Array.of(3.1416, 309.17, 412321.7, 1.45);
console.log(floatArr);

floatArr.set([]);
console.log(floatArr);

floatArr.set([ 314.16 ]);
console.log(floatArr);

floatArr.set([ 1024.2, 2056.3, 3069.17, 6666.69 ]);
console.log(floatArr);

floatArr.set([ 5024.97 ], 2);
console.log(floatArr);

floatArr.set([ '', '1099.13' ]);
console.log(floatArr);

console.log();

