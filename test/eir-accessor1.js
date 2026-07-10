// object-literal accessors through EIR (define_accessor); a get/set
// PAIR for one property must keep both halves

function pair() {
    let backing = 5;
    let o = {
        tag: "t",
        get n() { return backing * 10; },
        set n(v) { backing = v + 1; },
    };
    let before = o.n;
    o.n = 4;
    return o.tag + ":" + before + "," + o.n;
}

function getterOnly() {
    let i = 0;
    let o = { get next() { return i++; } };
    return o.next + "," + o.next + "," + o.next;
}

function setterOnly() {
    let log = [];
    let o = { set sink(v) { log.push(v); } };
    o.sink = "a";
    o.sink = "b";
    return log.join(",") + "/" + o.sink;
}

function mixedOrder() {
    let o = {
        a: 1,
        get b() { return this.a + 10; },
        c: 2,
        set b(v) { this.a = v; },
        d: 3,
    };
    let r1 = o.b;
    o.b = 100;
    return r1 + "," + o.b + "," + o.c + "," + o.d;
}

console.log(pair());
console.log(getterOnly());
console.log(setterOnly());
console.log(mixedOrder());
