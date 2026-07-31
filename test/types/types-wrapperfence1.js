// external calls into the exported f reach the module-private g with
// values maam never analyzed.  f(7) crosses g's constant-pruned branch
// (s becomes "s", so "s" * 2 must be NaN); a trusted rewrite of the
// g-site inside f would unbox the string unguarded and print garbage.
// Output must be identical to the flag-off executable.
import { f } from "./types-wrapperfence1/lib";
console.log(f(7)); // NaN — the pruned branch, taken for real
console.log(f(2)); // 4
console.log(f("2")); // "2" > 5 is false -> s = "2" -> "2" * 2 = 4
