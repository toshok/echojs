var arr = Int32Array.of(101, 99, 77, 67, 4);
var iter;

// 1. Go for keys()
var keys = arr.keys();
while (!(iter = keys.next ()).done)
    console.log (iter.value);

// 2. Go for values()
var values = arr.values();
while (!(iter = values.next ()).done)
    console.log (iter.value);

