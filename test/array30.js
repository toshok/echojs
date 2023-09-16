// generator: babel-node

var arr = ["this", "is", "a", "dummy", "array"];
var iter;
console.log(arr.toString());

// 1. Go for keys()

var keys = arr.keys();
while (!(iter = keys.next()).done) console.log(iter.value);

// 2. Go for values()
var values = arr.values();
while (!(iter = values.next()).done) console.log(iter.value);

// 3. Go for entries()
var entries = arr.entries();
while (!(iter = entries.next()).done) console.log(iter.value.toString());
