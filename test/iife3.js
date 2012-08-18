if (typeof console === "undefined") { console = { log: print }; }

var i = 10;
var total = (function() {
     return i;
})();
console.log (total);
