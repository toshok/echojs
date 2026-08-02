// generator: esm

// `export * as ns from`: the namespace object rides a named export slot;
// member reads on it resolve at runtime through the module accessors.
import { util, own } from "./exportall2-mid";

console.log(util.X);
console.log(util.add(2, 3));
console.log(own);
util.bump();
util.bump();
console.log(util.counter);
