function counters() {
    let i = 0;
    let s = "5";
    console.log(i++);
    console.log(++i);
    console.log(s++);
    console.log(--i);
    let o = { n: 10, arr: [1, 2, 3] };
    o.n++;
    o.arr[1]--;
    console.log(o.n, o.arr[1]);
}

function compounds() {
    let x = 1;
    x += 2; x *= 3; x -= 1; x %= 5; x <<= 2; x |= 1; x ^= 2; x >>= 1;
    console.log(x);
    let s = "a";
    s += "b";
    console.log(s);
    let o = { v: 7 };
    o.v += 3;
    o["v"] -= 1;
    console.log(o.v);
}

function templates(a, b) {
    console.log(`plain`);
    console.log(``.length);
    console.log(`a=${a} b=${b}`);
    console.log(`nested ${a > 1 ? `big ${a}` : "small"}!`);
}

function switches(x) {
    let r = "";
    switch (x) {
        case 1: r += "one ";
        case 2: r += "two "; break;
        case 3: r += "three "; break;
        default: r += "other ";
    }
    return r;
}

function forofs(arr) {
    let sum = 0;
    for (let v of arr) {
        if (v < 0) continue;
        if (v > 99) break;
        sum += v;
    }
    let last;
    for (last of arr) {}
    return `${sum}:${last}`;
}

function withDefaults(a, b = a + 1, c = "x") {
    return `${a},${b},${c}`;
}

var doubler = (x) => x * 2;
var describe = (n) => { if (n % 2 === 0) return `even ${n}`; return `odd ${n}`; };

function arrows(arr) {
    let big = arr.map(doubler).map((v) => v + 1);
    console.log(big.join(","));
    console.log(describe(4), describe(5));
}

counters();
compounds();
templates(2, "z");
console.log(switches(1), "|", switches(3), "|", switches(9));
console.log(forofs([1, 2, -5, 3, 200, 4]));
console.log(withDefaults(1), "|", withDefaults(1, 5), "|", withDefaults(1, undefined, "y"));
arrows([1, 2, 3]);
