// generator: babel-node

var buffer = new ArrayBuffer(8);
var uint8 = new Uint8Array(buffer);
uint8[0] = 1;
uint8[1] = 2;
uint8[2] = 3;

// [ 1, 2, 3, 0, 0, 0, 0, 0 ]
for (let i = 0; i < uint8.length; i++)
    console.log(uint8[i]);

console.log();

var sub = uint8.subarray(0, 4); 

// [ 1, 2, 3, 0 ]
for (let i = 0; i < sub.length; i++)
    console.log(sub[i]);

