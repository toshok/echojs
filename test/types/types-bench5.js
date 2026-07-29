// runtime-P2 microbenchmark driver: the types-bench1 workload, but the
// kernel lives in another module and is reached through its export —
// every call crosses the module boundary into the wrapper.
import { kernel } from "./types-bench5/lib";
var out = 0;
var r = 0;
while (r < 40) {
    out = out + kernel(1000000);
    r = r + 1;
}
console.log(out);
