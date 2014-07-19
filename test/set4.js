
var s = new Set ();
s.add ("lasagna");
s.add ("pizza");
s.add ("pasta");
s.add ("pizza");
s.add ("salad");

// Simple iterator with the for..of statement.
for (var value of s)
    console.log (value);

