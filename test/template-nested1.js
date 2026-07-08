// generator: none
function f(xs) {
    return `(${xs.map((p) => `${p}!`).join(", ")})`;
}
console.log(f(["a", "b"]));
console.log(`x${`y${1 + 1}z`}w`);
