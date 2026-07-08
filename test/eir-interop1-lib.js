export let counter = 0;
const K = 21;
export const NAME = "interop";

export function bump() {
    counter = counter + 1;
    return counter;
}

export function doubled() {
    return K * 2;
}

export function helper(x) {
    return x + 1;
}

export function viaHelper() {
    return helper(41);
}

var fact = function (n) {
    if (n < 2) return 1;
    return n * fact(n - 1);
};

export function fact5() {
    return fact(5);
}
