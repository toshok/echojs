// external calls cross the module boundary into the wrapped export:
// numbers pass the has_tag chain into the guarded clone; the string and
// the missing-arg call fail it and run the original generic body.
// Output must be identical to the flag-off executable in every case.
import { kernel } from "./types-wrapper1/lib";
console.log(kernel(10));
console.log(kernel(20.5));
console.log(kernel("3")); // guard fails -> generic path, coercing compare
console.log(kernel()); // missing arg is undefined -> guard fails
