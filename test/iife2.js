var i;
var total = 0;
for (i = 0; i < 100; i = i + 1) {
    (function () {
        total = total + i;
    })();
}
console.log(total);
