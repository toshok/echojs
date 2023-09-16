// generator: babel-node

function foo({ x, y }) {
    console.log(x + " + " + y);
}
foo({ x: "hello", y: "world" });

function bar({ x: { y, z }, w }) {
    console.log(y + " + " + z + " + " + w);
}
bar({ x: { y: "goodbye", z: "cruel" }, w: "world" });

function foo2([x, y]) {
    console.log(x + " + " + y);
}
foo2(["hello", "world"]);

function bar2([{ y, z }, w]) {
    console.log(y + " + " + z + " + " + w);
}
bar2([{ y: "goodbye", z: "cruel" }, "world"]);

function bar3([[y, z], w]) {
    console.log(y + " + " + z + " + " + w);
}
bar3([["goodbye", "cruel"], "world"]);
