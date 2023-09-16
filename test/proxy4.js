// generator: none
// skip-if: true

// from MDN
let products = new Proxy(
    [
        { name: "Firefox", type: "browser" },
        { name: "SeaMonkey", type: "browser" },
        { name: "Thunderbird", type: "mailer" },
    ],
    {
        get: function (obj, prop) {
            // The default behavior to return the value; prop is usually an integer
            if (prop in obj) {
                return obj[prop];
            }

            // Get the number of products; an alias of products.length
            if (prop === "number") {
                return obj.length;
            }

            let result,
                types = {};

            for (let product of obj) {
                if (product.name === prop) {
                    result = product;
                }
                if (types[product.type]) {
                    types[product.type].push(product);
                } else {
                    types[product.type] = [product];
                }
            }

            // Get a product by name
            if (result) {
                return result;
            }

            // Get products by type
            if (prop in types) {
                return types[prop];
            }

            // Get product types
            if (prop === "types") {
                return Object.keys(types);
            }

            return undefined;
        },
    }
);

console.log(products[0]); // { name: 'Firefox', type: 'browser' }
console.log(products["Firefox"]); // { name: 'Firefox', type: 'browser' }
console.log(products["Chrome"]); // undefined
console.log(products.browser); // [{ name: 'Firefox', type: 'browser' }, { name: 'SeaMonkey', type: 'browser' }]
console.log(products.types); // ['browser', 'mailer']
console.log(products.number); // 3
