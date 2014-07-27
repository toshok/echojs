var fs = require('fs');

console.log(fs.statSync('stat1.js').isFile());
console.log(fs.statSync('stat1.js').isDirectory());
