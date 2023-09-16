// generator: babel-node

function Multiplier(factor) {
    this.factor = factor;
}
Multiplier.prototype.multiply = function (x) {
    if (Array.isArray(x)) {
        return x.map((el) => el * this.factor);
    } else {
        let mult = (x) => {
            return x * this.factor;
        };
        return mult(x);
    }
};

let mult = new Multiplier(10);

console.log(mult.multiply([1, 2, 3]));
console.log(mult.multiply(1));
