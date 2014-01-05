
var buff = new ArrayBuffer (16);
var intarr = new Int32Array (buff);
var shortarr = new Int16Array (buff);

function dumpStuff (arr)
{
  console.log (" byteOffset: " + arr.byteOffset);
  console.log (" byteLength: " + arr.byteLength);
  console.log (" length: " + arr.length);
}

console.log ("ArrayBuffer byte Length: " + buff.byteLength);
console.log ("Int32Array:");
dumpStuff (intarr);
console.log ("Int16Array:");
dumpStuff (shortarr);

console.log ("Int32Array (all)");
var intarr2 = new Int32Array (buff, 0, intarr.length);
dumpStuff (intarr2);

console.log ("Int32Array (4, 2):");
var intarr3 = new Int32Array (buff, 4, 2);
dumpStuff (intarr3);

try {
  new Int32Array (buff, 4, 4);
  console.log ("FAIL #1");
} catch (e) {
  console.log (e);
}

