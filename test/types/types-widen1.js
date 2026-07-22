// reassignment widening: these do NOT diamond (documented behavior).
// `w` holds number THEN string -> the oracle reports num|str for every
// node mapped to it; `u` starts undefined -> number|undefined.  Only
// exact {number} qualifies, so expect diamonds=0 — correctness must
// hold regardless (the generic ops run).
var w = 1;
console.log(w + 1);
w = "s";
console.log(w + "!");
var u;
u = 2;
console.log(u + 3);
