
var s = new Set ();
s.add ("lasagna");
s.add ("pizza");
s.add ("pasta");
s.add ("pizza");
s.add ("salad");

var values = s.values();
var iter;
while (!(iter = values.next()).done)
    console.log (iter.value);

var entries = s.entries();
while (!(iter = entries.next()).done)
    console.log (iter.value);

