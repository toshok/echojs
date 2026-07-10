// generator.return(): finally blocks run, the return value lands in the
// final iteration result, and completed generators answer correctly.

function* g() {
    try {
        yield 1;
        yield 2;
    } finally {
        console.log("fin");
    }
}

let it = g();
console.log(JSON.stringify(it.next()));
console.log(JSON.stringify(it.return(5)));
console.log(JSON.stringify(it.next()));

// the body's return value is the final result's value
function* h() {
    yield 1;
    return 42;
}
let it2 = h();
console.log(JSON.stringify(it2.next()));
console.log(JSON.stringify(it2.next()));
console.log(JSON.stringify(it2.next()));

// .return before the first next: the body never runs
let it3 = g();
console.log(JSON.stringify(it3.return(9)));
console.log(JSON.stringify(it3.next()));

// .throw resumes at the yield: catch and finally both run
function* k() {
    try {
        yield 1;
    } catch (e) {
        console.log("caught", e);
    } finally {
        console.log("kfin");
    }
    console.log("after");
}
let it4 = k();
console.log(JSON.stringify(it4.next()));
console.log(JSON.stringify(it4.throw("x")));

// .throw at a never-started generator throws in the caller
let it5 = k();
try {
    it5.throw("early");
} catch (e) {
    console.log("caller caught", e);
}
