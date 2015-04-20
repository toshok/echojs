// Proxy preventExtensions handler from kangax
var proxied = {};
var passed = false;
Object.preventExtensions(
    new Proxy(proxied, {
        preventExtensions: function (t) {
            passed = t === proxied;
            return Object.preventExtensions(proxied);
        }
    })
);
console.log(passed);
