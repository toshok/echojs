
console.log(Object.is());
console.log(Object.is(undefined, undefined));
console.log(Object.is(undefined, null));
console.log(Object.is(null, null));
console.log(Object.is(NaN, NaN));
console.log(Object.is(5, 5));
console.log(Object.is("hi", "h" + "i"));
console.log(Object.is("hi", new String("hi")));
