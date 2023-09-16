// various tests for range errors in ctor calls

try {
    new Int8Array(new ArrayBuffer(9));
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Int16Array(new ArrayBuffer(9));
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Uint16Array(new ArrayBuffer(9));
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Int32Array(new ArrayBuffer(9));
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Uint32Array(new ArrayBuffer(9));
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Float32Array(new ArrayBuffer(9));
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Float64Array(new ArrayBuffer(9));
    console.log("works");
} catch (e) {
    console.log(e.name);
}

try {
    new Uint8Array(new ArrayBuffer(9), 1);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Int16Array(new ArrayBuffer(9), 1);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Uint16Array(new ArrayBuffer(9), 1);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Int32Array(new ArrayBuffer(9), 1);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Uint32Array(new ArrayBuffer(9), 1);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Float32Array(new ArrayBuffer(9), 1);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Float64Array(new ArrayBuffer(9), 1);
    console.log("works");
} catch (e) {
    console.log(e.name);
}

try {
    new Uint8Array(new ArrayBuffer(6));
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Int16Array(new ArrayBuffer(6));
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Uint16Array(new ArrayBuffer(6));
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Int32Array(new ArrayBuffer(6));
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Uint32Array(new ArrayBuffer(6));
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Float32Array(new ArrayBuffer(6));
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Float64Array(new ArrayBuffer(6));
    console.log("works");
} catch (e) {
    console.log(e.name);
}

try {
    new Uint8Array(new ArrayBuffer(16), 1);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Int16Array(new ArrayBuffer(16), 1);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Uint16Array(new ArrayBuffer(16), 1);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Int32Array(new ArrayBuffer(16), 1);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Uint32Array(new ArrayBuffer(16), 1);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Float32Array(new ArrayBuffer(16), 1);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Float64Array(new ArrayBuffer(16), 1);
    console.log("works");
} catch (e) {
    console.log(e.name);
}

try {
    new Uint8Array(new ArrayBuffer(16), 2);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Int16Array(new ArrayBuffer(16), 2);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Uint16Array(new ArrayBuffer(16), 2);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Int32Array(new ArrayBuffer(16), 2);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Uint32Array(new ArrayBuffer(16), 2);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Float32Array(new ArrayBuffer(16), 2);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Float64Array(new ArrayBuffer(16), 2);
    console.log("works");
} catch (e) {
    console.log(e.name);
}

try {
    new Uint8Array(new ArrayBuffer(16), 2);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Int16Array(new ArrayBuffer(16), 2);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Uint16Array(new ArrayBuffer(16), 2);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Int32Array(new ArrayBuffer(16), 4);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Uint32Array(new ArrayBuffer(16), 4);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Float32Array(new ArrayBuffer(16), 4);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
try {
    new Float64Array(new ArrayBuffer(16), 8);
    console.log("works");
} catch (e) {
    console.log(e.name);
}
