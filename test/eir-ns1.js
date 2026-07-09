// generator: none
import * as lib from "./eir-ns1-lib";
import dflt from "./eir-ns1-lib";

function useNs(name) {
    let g = lib.greet(name);
    return `${g}/${lib.LIMIT}`;
}

function useNsState() {
    lib.bump();
    lib.bump();
    return lib.seen;
}

function useDefault(x) {
    return dflt(x);
}

console.log(useNs("eir"));
console.log(useNsState());
console.log(useDefault(7));
