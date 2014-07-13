
var s = new Set ();
s.add ("lasagna");
s.add ("pizza");
s.add ("pasta");
s.add ("pizza");
s.add ("salad");

function print_values (values) {
    var iter;
    while (!(iter = values.next()).done)
        console.log (iter.value);
}

print_values (s.values());
print_values (s.entries());
console.log ();

s.delete ("pasta");
print_values (s.values());
console.log ();

s.delete("lasagna");
print_values (s.values());
console.log ();

s.delete("salad");
print_values(s.values());
console.log ();

s.delete("pizza");
print_values(s.values());

