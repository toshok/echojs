// generator: none
// skip-if: true
// babel/traceur don't throw an exception, and unfortunately neither do we

// "can't use 'this' with 'new'" from kangax
function* generator() {
    yield this.x;
    yield this.y;
}
try {
    new generator().next();
    console.log(false);
} catch (e) {
    console.log(true);
}
