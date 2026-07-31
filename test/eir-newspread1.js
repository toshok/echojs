// new Foo(...args) — never compiled before (%constructApply)

function Point(x, y, z) { this.sum = x + y + z; this.len = arguments.length; }

function spreadNew(xs) {
    let p = new Point(...xs);
    return p.sum + "/" + p.len;
}

function mixedNew(xs) {
    let p = new Point(1, ...xs);
    return p.sum + "/" + p.len;
}

function litOnlyNew() {
    let p = new Point(...[7, 8], 9);
    return p.sum + "/" + p.len;
}

class Tagged { constructor(...parts) { this.tag = parts.join("-"); } }
function classNew(xs) { return new Tagged(...xs, "end").tag; }

console.log(spreadNew([1, 2, 3]));
console.log(mixedNew([10, 20]));
console.log(litOnlyNew());
console.log(classNew(["a", "b"]));
console.log(new Point(...[4], 5, ...[6]) instanceof Point);
