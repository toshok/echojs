// the wrong-oracle probe for the P4.6 2-way polymorphic chain: lib.js's
// oracle typed sum/setx's receiver with TWO terminal shapes from its
// module-local calls, so both classes ride their own fast arm.  Cross-
// module we hand the chain receivers it never saw — a repr-mismatched
// object, a third shape, a dictionary-mode (post-delete) object — and
// every one must route through the shared slow path with node-identical
// output; correctness never depends on the oracle being right.
import { sum, setx, mk2, mk3 } from "./types-poly1/lib";
console.log(sum(mk2(1, 2)));            // arm-1 fast: 3
console.log(sum(mk3(10, 20, 30)));      // arm-2 fast: 30
console.log(sum({ x: "a", y: "b" }));   // repr mismatch: slow, "ab"
console.log(sum({ x: 1, y: 2, w: 3 })); // a third shape: slow, 3
var del = { x: 100, y: 200 };
delete del.x; del.x = 7;                // dictionary mode: guards fail
console.log(sum(del));                  // 207
console.log(setx(mk3(1, 2, 3), 42));    // arm-2 typed store: 42
console.log(setx({ x: "s", y: 0 }, "t")); // non-number into the chain: slow, "t"
console.log(setx(del, 9));              // dictionary store: slow, 9
