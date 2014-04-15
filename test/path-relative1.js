var path = require("path");

console.log (path.relative ('/foo/bar/baz', '/foo/bar/bling'));
console.log (path.relative('/data/orandea/test/aaa', '/data/orandea/impl/bbb'));
console.log (path.relative('../hi', '../bye'));
