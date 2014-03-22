
let i = 0;
let foo = {
  get getter () { return i++; },
  get func()    { return function () { console.log ("hi"); }; }
};

console.log(foo.getter);
console.log(foo.getter);
console.log(foo.getter);
console.log(foo.getter);
console.log(foo.getter);
console.log(foo.func);
console.log(foo.func());
