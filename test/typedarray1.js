var arrayBufferSize = 200;
var offsetElements = 10;

function dumpStuff(a) {
    console.log("length = " + a.length);
    console.log("byteOffset = " + a.byteOffset);
    console.log("byteLength = " + a.byteLength);
}

console.log("Int8Array");
console.log(Int8Array.BYTES_PER_ELEMENT);
dumpStuff(new Int8Array());
dumpStuff(new Int8Array(10));
dumpStuff(new Int8Array(new ArrayBuffer(arrayBufferSize)));
dumpStuff(
    new Int8Array(new ArrayBuffer(arrayBufferSize), offsetElements * Int8Array.BYTES_PER_ELEMENT)
);

console.log("Uint8Array");
dumpStuff(new Uint8Array());
dumpStuff(new Uint8Array(10));
dumpStuff(new Uint8Array(new ArrayBuffer(arrayBufferSize)));
dumpStuff(
    new Uint8Array(new ArrayBuffer(arrayBufferSize), offsetElements * Uint8Array.BYTES_PER_ELEMENT)
);

console.log("Int16Array");
dumpStuff(new Int16Array());
dumpStuff(new Int16Array(10));
dumpStuff(new Int16Array(new ArrayBuffer(arrayBufferSize)));
dumpStuff(
    new Int16Array(new ArrayBuffer(arrayBufferSize), offsetElements * Int16Array.BYTES_PER_ELEMENT)
);

console.log("Uint16Array");
dumpStuff(new Uint16Array());
dumpStuff(new Uint16Array(10));
dumpStuff(new Uint16Array(new ArrayBuffer(arrayBufferSize)));
dumpStuff(
    new Uint16Array(
        new ArrayBuffer(arrayBufferSize),
        offsetElements * Uint16Array.BYTES_PER_ELEMENT
    )
);

console.log("Int32Array");
dumpStuff(new Int32Array());
dumpStuff(new Int32Array(10));
dumpStuff(new Int32Array(new ArrayBuffer(arrayBufferSize)));
dumpStuff(
    new Int32Array(new ArrayBuffer(arrayBufferSize), offsetElements * Int32Array.BYTES_PER_ELEMENT)
);

console.log("Uint32Array");
dumpStuff(new Uint32Array());
dumpStuff(new Uint32Array(10));
dumpStuff(new Uint32Array(new ArrayBuffer(arrayBufferSize)));
dumpStuff(
    new Uint32Array(
        new ArrayBuffer(arrayBufferSize),
        offsetElements * Uint32Array.BYTES_PER_ELEMENT
    )
);

console.log("Float32Array");
dumpStuff(new Float32Array());
dumpStuff(new Float32Array(10));
dumpStuff(new Float32Array(new ArrayBuffer(arrayBufferSize)));
dumpStuff(
    new Float32Array(
        new ArrayBuffer(arrayBufferSize),
        offsetElements * Float32Array.BYTES_PER_ELEMENT
    )
);

console.log("Float64Array");
dumpStuff(new Float64Array());
dumpStuff(new Float64Array(10));
dumpStuff(new Float64Array(new ArrayBuffer(arrayBufferSize)));
dumpStuff(
    new Float64Array(
        new ArrayBuffer(arrayBufferSize),
        offsetElements * Float64Array.BYTES_PER_ELEMENT
    )
);
