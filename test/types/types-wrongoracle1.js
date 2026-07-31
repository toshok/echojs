// the wrong-oracle guard: lib.js's oracle typed inc's param {number}
// (its only module-local call is numeric), but cross-module linking is
// unmodeled — we call it with a string.  The has_tag guard must route
// to the slow path and produce "x1": correctness never depends on the
// oracle being right.
import { inc } from "./types-wrongoracle1/lib";
console.log(inc("x"));
console.log(inc(1.5));
