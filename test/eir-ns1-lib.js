export function greet(name) {
    return "hi " + name;
}
export const LIMIT = 10;
export let seen = 0;
export function bump() {
    seen = seen + 1;
    return seen;
}
export default function dfltFn(x) {
    return x * 100;
}
