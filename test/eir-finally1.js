function order(g, log) {
    try {
        log.push("try");
        g();
        log.push("after");
    } finally {
        log.push("finally");
    }
    return log.join(",");
}

function retThrough(v) {
    let log = [];
    function inner() {
        try {
            return "ret:" + v;
        } finally {
            log.push("fin");
        }
    }
    return inner() + "/" + log.join(",");
}

function breakThrough(xs) {
    let seen = [];
    for (let i = 0; i < xs.length; i++) {
        try {
            if (xs[i] < 0) break;
            seen.push(xs[i]);
        } finally {
            seen.push("f" + i);
        }
    }
    return seen.join(",");
}

function nested() {
    let log = [];
    function inner() {
        try {
            try {
                return "v";
            } finally {
                log.push("f1");
            }
        } finally {
            log.push("f2");
        }
    }
    return inner() + "/" + log.join(",");
}

function override() {
    try {
        return "from-try";
    } finally {
        return "from-finally";
    }
}

function excPath(log) {
    try {
        try {
            throw new Error("boom");
        } finally {
            log.push("fin");
        }
    } catch (e) {
        log.push("caught:" + e.message);
    }
    return log.join(",");
}

console.log(order(function () {}, []));
try { console.log(order(function () { throw new Error("x"); }, [])); } catch (e) { console.log("threw"); }
console.log(retThrough(7));
console.log(breakThrough([5, 6, -1, 9]));
console.log(nested());
console.log(override());
console.log(excPath([]));
