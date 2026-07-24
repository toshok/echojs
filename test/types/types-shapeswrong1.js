// the wrong-oracle shape guard (shapes-plan P4.3): lib.js's oracle typed
// sumxy's receiver with the terminal shape {x: num, y: num} from its only
// module-local call, but cross-module we hand it (a) a string-valued
// object with different reprs, (b) an object with extra fields, (c) a
// dictionary-mode object (post-delete), and (d) the shape-matching case.
// Every access must route through the guard (fast only when the runtime
// shape matches) with node-identical output — correctness never depends
// on the oracle being right.
import { sumxy, mk } from "./types-shapeswrong1/lib";
console.log(sumxy({ x: "a", y: "b" }));      // repr mismatch: slow, "ab"
var wide = { x: 10, y: 20, z: 30 };
console.log(sumxy(wide));                    // extra field: guard fails, 30
var del = { x: 100, y: 200 };
delete del.x; del.x = 7;                     // dictionary mode: guard fails
console.log(sumxy(del));
console.log(sumxy(mk(3, 4)));                // the matching shape: fast, 7
