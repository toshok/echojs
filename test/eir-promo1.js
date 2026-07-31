// generator: none
import { tick, readAll, viaValue, makeCloser } from "./eir-promo1-lib";

console.log(tick());
console.log(tick());
console.log(readAll());
console.log(viaValue());
let c = makeCloser();
console.log(c());
console.log(readAll());
