
let a, b, c, d;
[a, b] = ["hello", "world"];
console.log(a, b);

// need ()'s around this to force it to be parsed as an assignment expression
// and not as $object_literal ... the-rest
({c, d} = {c: "hello", d: "world"});
console.log(c, d);
