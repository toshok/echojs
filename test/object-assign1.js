
let from1 = Object.create(null);
from1.prop = 5;

let from2 = Object.create(null);
from2.prop2 = 7;

let to = Object.create(null);
Object.assign(to, from1, from2);

console.log(to.prop);
console.log(to.prop2);
