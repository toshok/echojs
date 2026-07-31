// tagged template literals through EIR (template_callsite)

function tag(strings, ...subs) {
    return strings.join("|") + "/" + strings.raw.join("|") + "/" + subs.join(",");
}

function basic(x) {
    return tag`a ${x} b ${x * 2} c`;
}

let callsites = [];
function collect(strings) { callsites.push(strings); return "ok"; }
function identity(n) {
    for (let i = 0; i < n; i++) collect`same site`;
    return callsites.length === n && callsites.every(function (c) { return c === callsites[0]; });
}

let o = {
    prefix: ">>",
    m(strings, v) { return this.prefix + strings[0] + v; },
};
function methodTag(v) {
    return o.m`lead ${v}`;
}

function rawEscapes() {
    return tag`x\n${1}`;
}

console.log(basic(5));
console.log(identity(3));
console.log(methodTag(9));
console.log(rawEscapes());
console.log(tag`only literal`);
