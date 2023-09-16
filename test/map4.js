// generator: babel-node

var m = new Map();
m.set("one", "uno");
m.set("two", "dos");
m.set("three", "tres");
m.set("four", "cuatro");

var iter;

var keys = m.keys();
while ((iter = keys.next()).done != true) console.log(iter.value);

var values = m.values();
while ((iter = values.next()).done != true) console.log(iter.value);

var entries = m.entries();
while ((iter = entries.next()).done != true) console.log(iter.value.toString());

for (let e of m) console.log(e.toString());
