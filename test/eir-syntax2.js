// generator: none
import { makeTag, twice, inc, incTwice } from "./eir-syntax2-lib";

function forins(o) {
    let ks = [];
    for (let k in o) {
        if (k === "skip") continue;
        ks.push(k);
    }
    let k2;
    for (k2 in o) {}
    return ks.join(",") + "|" + k2;
}

function rests(a, ...xs) {
    return `${a}:${xs.length}:${xs.join("-")}`;
}

function restOnly(...xs) {
    return xs.map((x) => x * 2).join(",");
}

function regexes(s) {
    let re = /a(b+)c/i;
    console.log(re.test(s));
    console.log(s.replace(/b+/g, "B"));
    let m = s.match(/a(b+)c/);
    console.log(m ? m[1] : "none");
}

console.log(forins({ x: 1, skip: 2, y: 3 }));
console.log(rests(9), "|", rests(9, 1), "|", rests(9, 1, 2, 3));
console.log(restOnly(1, 2, 3));
regexes("xxabbbcyy");
console.log(twice(inc, 5));
console.log(incTwice(10));
console.log(makeTag("div"));
