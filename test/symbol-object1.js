var symbol = Symbol();
var symbolObject = Object(symbol);

console.log(typeof symbolObject === "object" &&
  symbolObject == symbol &&
  symbolObject.valueOf() === symbol);

