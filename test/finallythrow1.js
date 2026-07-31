function finOnly() {
    try {
        throw new Error("boom");
    } finally {
        console.log("finally ran");
    }
    return "SWALLOWED";
}

function forLetThrow(items) {
    for (let i = 0; i < items.length; i++) {
        if (items[i] === 3) throw new Error("found:" + i);
    }
    return "no throw";
}

function nestedFinally() {
    let order = [];
    try {
        try {
            throw new Error("inner");
        } finally {
            order.push("f1");
        }
    } finally {
        order.push("f2");
    }
    return order.join(",");
}

function finallyBreak(items) {
    let seen = [];
    for (let i = 0; i < items.length; i++) {
        try {
            if (items[i] < 0) break;
            seen.push(items[i]);
        } finally {
            seen.push("f" + i);
        }
    }
    return seen.join(",");
}

function finallyReturn() {
    try {
        return "from-try";
    } finally {
        console.log("fr ran");
    }
}

try { console.log(finOnly()); } catch (e) { console.log("caught: " + e.message); }
try { console.log(forLetThrow([1, 2, 3])); } catch (e) { console.log("caught: " + e.message); }
try { console.log(nestedFinally()); } catch (e) { console.log("caught: " + e.message); }
console.log(finallyBreak([5, 6, -1, 7]));
console.log(finallyReturn());
console.log(forLetThrow([1, 2]));
