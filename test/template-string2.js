
function msg(callsiteId, substitutions) {
  var strs = [];
  for (var i = 0, e = callsiteId.cooked.length; i < e; i ++) {
    strs.push(callsiteId.cooked[i]);
    if (i < e - 1)
      strs.push(substitutions[i]);
  }
  return strs.join("");
}

var mundo = "world";

console.log (msg`Hello, ${mundo}!`);

