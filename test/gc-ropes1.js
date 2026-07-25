var parts = [];
for (var i = 0; i < 50; i++) parts.push("x" + i);
var s = "";
for (var i = 0; i < 50; i++) s = s + "," + parts[i];
console.log(s.length);
console.log(s.substring(0, 30));
console.log(s === s.split("").join(""));
