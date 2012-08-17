if (typeof console === "undefined") { console = { log: print }; }

var i;
var total = 0;
for (i = 0; i < 1000000; i = i + 1) {
  (function() {
     total = total + i;
   })();
}
console.log (total);
