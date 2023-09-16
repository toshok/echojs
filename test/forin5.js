// xfail: we enumerate the entire range 0..length of an array, including the holes.

var a = [];
a[10] = "hi";

for (var k in a) {
    console.log(k + " = " + a[k]);
}
