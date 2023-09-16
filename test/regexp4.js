var str = "a1a2a3a4a5";
str = str.replace(/\d/g, "");
console.log("'" + str + "'");

var str = "a1a2a3a4a5";
str = str.replace(new RegExp("\\d", "g"), "");
console.log("'" + str + "'");
