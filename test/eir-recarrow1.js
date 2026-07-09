function countdown() {
    let walk = (n) => {
        if (n <= 0) return 0;
        return walk(n - 1) + 1;
    };
    return walk(5);
}

function namedRec() {
    let visit = function (n) {
        if (n === 0) return "done";
        return visit(n - 1);
    };
    return visit(3);
}

function walkTree(root) {
    let seen = [];
    let walk = (n) => {
        if (!n || typeof n !== "object") return;
        if (Array.isArray(n)) {
            for (let el of n) walk(el);
            return;
        }
        if (n.name) seen.push(n.name);
        for (let k of Object.keys(n)) walk(n[k]);
    };
    walk(root);
    return seen.join(",");
}

console.log(countdown());
console.log(namedRec());
console.log(walkTree({ name: "a", kids: [{ name: "b" }, { name: "c", kids: [{ name: "d" }] }] }));
