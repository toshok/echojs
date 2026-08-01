// generator: esm

// `export * from` re-export: direct names, a diamond (BASE reaches the
// hub through two stars that resolve to the same original export), an
// explicit export shadowing a star name, and a star-of-a-star.
import { A, mkA, B, mkB, BASE, override, local } from "./exportall1-hub";
import { A as outerA, local as outerLocal } from "./exportall1-outer";

console.log(A);
console.log(mkA(1));
console.log(B);
console.log(mkB(2));
console.log(BASE);
console.log(override);
console.log(local);
console.log(outerA);
console.log(outerLocal);
