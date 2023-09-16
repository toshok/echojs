// generator: babel-node

// from kangax's table

console.log(
    (function () {
        try {
            var object = {};
            var symbol = Symbol();
            var value = Math.random();
            object[symbol] = value;

            return (
                object[symbol] === value &&
                Object.keys(object).length === 0 &&
                Object.getOwnPropertyNames(object).length === 0
            );
        } catch (e) {
            return false;
        }
    })()
);
