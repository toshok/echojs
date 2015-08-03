// xfail: we use %g to format floats/doubles, and differ when printing numbers.

// various tests for range errors in ctor calls

var buf = new ArrayBuffer(24);

var darray = new Float64Array(buf);
var farray = new Float32Array(buf);
var iarray = new Int32Array(buf);
var sarray = new Int16Array(buf);
var barray = new Uint8Array(buf);

darray[0] = 3.1415926535;

console.log (darray[0]);
console.log (farray[0]);
console.log (iarray[0]);
console.log (sarray[0]);
console.log (barray[0]);
