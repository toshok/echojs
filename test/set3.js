// generator: babel-node

var s = new Set();
s.add("lasagna");
s.add("pizza");
s.add("pasta");
s.add("pizza");
s.add("salad");

function print_values(values) {
    var iter;
    while (!(iter = values.next()).done) console.log(iter.value);
}
// XXX this function shouldn't be here, but we need to workaround a bug in ejs
function print_entries(values) {
    var iter;
    while (!(iter = values.next()).done) console.log(iter.value.toString());
}

print_values(s.values());
print_entries(s.entries());
print_values(s.keys());
console.log();

s.delete("pasta");
print_values(s.values());
console.log();

s.delete("lasagna");
print_values(s.values());
console.log();

s.delete("salad");
print_values(s.values());
console.log();

s.delete("pizza");
print_values(s.values());
