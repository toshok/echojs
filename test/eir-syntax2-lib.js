export function makeTag(name) {
    return "<" + name + ">";
}
export function twice(f, x) {
    return f(f(x));
}
export function inc(x) {
    return x + 1;
}
export function incTwice(x) {
    return twice(inc, x);
}
