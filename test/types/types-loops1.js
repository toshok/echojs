// for-loop counters and < in loop conditions
var total = 0;
for (var i = 0; i < 10; i = i + 1) {
    total = total + i * i;
}
var j = 0;
while (j < 5) { j = j + 1; }
console.log(total);
console.log(j);
console.log(i < j);
