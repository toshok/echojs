// generator: none

import * as path from '@node-compat/path';

console.log(path.resolve ("/hi/there", "./bar"));
console.log(path.resolve ("/hi/there", "../bar"));
console.log(path.resolve ("/hi/there", "../../bar"));
console.log(path.resolve ("/hi/there", "../../../bar"));
console.log(path.resolve ("/hi/there", "../../../../bar"));
console.log(path.resolve ("/hi/there", "../../../../bar/fweep"));
console.log(path.resolve ("/hi//there", "bar"));
console.log(path.resolve ("/hi////there", "bar"));
console.log(path.resolve ("/hi/there", "/fweep"));
