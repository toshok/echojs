// generator: none
// proxy isExtensible handler test from kangax
var proxied = {};
var passed = false;
Object.isExtensible(
    new Proxy(proxied, {
        isExtensible: function (t) {
            passed = t === proxied;
            return true;
        },
    })
);
console.log(passed);
