// xfail: we use %g to format floats/doubles, and differ when printing numbers.

var buff = new ArrayBuffer (16);
var view = new DataView (buff);

view.setInt32 (0, 1, true);
view.setInt32 (4, 3, false);
view.setInt32 (8, 5, true);
view.setInt32 (12, 7, false);

console.log (view.getInt32 (0, true));
console.log (view.getInt32 (4, false));
console.log (view.getInt32 (8, true));
console.log (view.getInt32 (12, false));

console.log (view.getInt32 (0, false));
console.log (view.getInt32 (4, true));
console.log (view.getInt32 (8, false));
console.log (view.getInt32 (12, true));

var view3 = new DataView (buff, 0, buff.byteLength);
view3.setInt32 (0, 111);
view3.setInt32 (4, 113);
view3.setInt32 (8, 117);
view3.setInt32 (12, 121);

console.log (view3.getInt32 (0));
console.log (view3.getInt32 (4));
console.log (view3.getInt32 (8));
console.log (view3.getInt32 (12));

view.setFloat64 (0, 3.1416, true);
view.setFloat64 (8, 666.999, false);

console.log (view.getFloat64 (0, true));
console.log (view.getFloat64 (8, false));

console.log (view.getFloat64 (0, false));
console.log (view.getFloat64 (8, true));
