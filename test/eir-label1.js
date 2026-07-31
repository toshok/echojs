// labeled statements and labeled break/continue through EIR

function labeledBreak(grid) {
    let found = "";
    outer: for (let i = 0; i < grid.length; i++) {
        for (let j = 0; j < grid[i].length; j++) {
            if (grid[i][j] < 0) { found = i + "," + j; break outer; }
        }
    }
    return found || "none";
}

function labeledContinue(n) {
    let out = [];
    outer: for (let i = 0; i < n; i++) {
        for (let j = 0; j < n; j++) {
            if (j > i) continue outer;
            out.push(i + "" + j);
        }
    }
    return out.join(",");
}

function labeledBlock(x) {
    let log = [];
    done: {
        log.push("a");
        if (x) break done;
        log.push("b");
    }
    log.push("c");
    return log.join(",");
}

function labeledThroughFinally(xs) {
    let log = [];
    outer: for (let i = 0; i < xs.length; i++) {
        try {
            if (xs[i] < 0) break outer;
            log.push("v" + xs[i]);
        } finally {
            log.push("f" + i);
        }
    }
    return log.join(",");
}

function labeledContinueThroughFinally(xs) {
    let log = [];
    outer: for (let i = 0; i < xs.length; i++) {
        inner: for (let j = 0; j < 2; j++) {
            try {
                if (xs[i] < 0) continue outer;
                log.push(i + ":" + j);
            } finally {
                log.push("f" + i + j);
            }
        }
    }
    return log.join(",");
}

function labeledSwitch(k) {
    let log = [];
    pick: switch (k) {
        case 1:
            log.push("one");
            if (k === 1) break pick;
            log.push("unreached");
        case 2:
            log.push("two");
    }
    log.push("after");
    return log.join(",");
}

function labeledWhile(n) {
    let c = 0;
    again: while (true) {
        c++;
        if (c < n) continue again;
        break again;
    }
    return c;
}

console.log(labeledBreak([[1, 2], [3, -1], [5]]));
console.log(labeledBreak([[1], [2]]));
console.log(labeledContinue(3));
console.log(labeledBlock(true));
console.log(labeledBlock(false));
console.log(labeledThroughFinally([7, -2, 9]));
console.log(labeledContinueThroughFinally([4, -5, 6]));
console.log(labeledSwitch(1));
console.log(labeledSwitch(2));
console.log(labeledWhile(4));
