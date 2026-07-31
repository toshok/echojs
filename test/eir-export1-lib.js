export default class Counter {
    constructor(n) {
        this.n = n;
    }
    inc() {
        return ++this.n;
    }
}

export const K = 7;

export function mk() {
    return "mk";
}
