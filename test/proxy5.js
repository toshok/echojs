// generator: none
// kangax compat test

console.log(
    new Proxy(
        {},
        {
            get: function () {
                return 5;
            },
        }
    ).foo === 5
);
