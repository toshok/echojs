// generator: none
// from MDN

var target = {};
var p = new Proxy(target, {});

p.a = 37; // operation forwarded to the proxy

console.log(target.a); // 37. The operation has been properly forwarded
