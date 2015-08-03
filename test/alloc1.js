// skip-if: true
// we don't bother running this, should probably just remove it.
// allocation heavy code

for (var i = 0; i < 10000000; i ++) {
  var s = "hi" + i;
}

if (typeof(console) === "object")
  console.log(s);
else
  print (s);
