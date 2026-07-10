// generator: babel-node
// `export default class`, default+named import, and re-export
import Counter, { K, mk } from "./eir-export1-lib";
export { mk as remk } from "./eir-export1-lib";

let c = new Counter(K);
console.log(c.inc());
console.log(c.inc());
console.log(K);
console.log(mk());
