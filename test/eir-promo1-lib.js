let counter = 0;
var state = { calls: 0 };
const registry = [];

var describe = function (tag) {
    return `${tag}:${counter}:${state.calls}`;
};

function useAsValue(f, tag) {
    return f(tag);
}

export function tick() {
    counter += 1;
    state.calls++;
    registry.push(counter);
    return counter;
}

export function readAll() {
    return `${counter}/${state.calls}/${registry.join(",")}/${describe("r")}`;
}

export function viaValue() {
    return useAsValue(describe, "v");
}

// legacy-side interop: toplevel code (legacy) mutates the same storage
counter = 100;
state.calls = 50;

export function makeCloser() {
    return function () {
        counter += 1000;
        return counter;
    };
}
