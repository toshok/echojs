import * as path from '@node-compat/path';

console.log (path.relative ('/foo/bar/baz', '/foo/bar/bling'));
console.log (path.relative('/data/orandea/test/aaa', '/data/orandea/impl/bbb'));
console.log (path.relative('../hi', '../bye'));
console.log (path.relative("/Users/toshok/src/coffeekit/echo-js/test", "/Users/toshok/src/coffeekit/echo-js/test/modules1/foo1"));
