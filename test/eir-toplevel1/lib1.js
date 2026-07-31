export const K = 7;
export let counter = 0;
export function inc(n) { counter += n; return counter; }
let hidden = "h";
export function peek() { return hidden + K; }
export default "DFLT";
