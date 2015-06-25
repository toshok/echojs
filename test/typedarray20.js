
// int8
let byteArr = Int8Array.of(1, 127, 66, 48);
console.log(byteArr);

byteArr.set(Int8Array.of());
console.log(byteArr);

byteArr.set(Int8Array.of(13));
console.log(byteArr);

byteArr.set(Int8Array.of(21, 39, 43, 66));
console.log(byteArr);

byteArr.set(Int8Array.of(68, 69), 1);
console.log(byteArr);

console.log();

// int32
let arr = Int32Array.of(1, 258, 1025, 2057);
console.log(arr);

arr.set(Int32Array.of());
console.log(arr);

arr.set(Int32Array.of(314));
console.log(arr);

arr.set(Int32Array.of(1024, 2056, 3069, 4044));
console.log(arr);

arr.set(Int32Array.of(666, 69), 1);
console.log(arr);

console.log();

// float64
let floatArr = Float64Array.of(3.1416, 309.17, 412321.7, 1.45);
console.log(floatArr);

floatArr.set(Float64Array.of());
console.log(floatArr);

floatArr.set(Float64Array.of(314.16));
console.log(floatArr);

floatArr.set(Float64Array.of(1024.2, 2056.3, 3069.17, 6666.69));
console.log(floatArr);

floatArr.set(Float64Array.of(5024.97), 2);
console.log(floatArr);

console.log();

