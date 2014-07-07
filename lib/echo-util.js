//let terminal = require('terminal');

module b from 'ast-builder';
import { Literal, CallExpression, Identifier } from 'ast-builder';

export function shallow_copy_object (o) {
    if (!o) return null;

    let new_o = Object.create(Object.getPrototypeOf(o));
    for (let x of Object.getOwnPropertyNames(o))
	new_o[x] = o[x];
    return new_o;
}
        
export function deep_copy_object (o) {
    return JSON.parse(JSON.stringify(o));
}

export function map (f, seq) {
    let rv = [];
    for (let el of seq) {
	rv.push (f(el));
    }
    return rv;
}

export function foldl (f, z, arr) {
    if (arr.length === 0) return z;

    return foldl(f, (f(z), arr[0]), arr.slice(1));
}

export function reject (o, pred) {
    let rv = Object.create(null);
    for (let prop of Object.getOwnPropertyNames(o)) {
	if (!pred(prop))
	    rv[prop] = o[prop];
    }
    return rv;
}
        
export function startGenerator() {
    let _gen = 0;
    return () => {
        let id = _gen;
        _gen += 1;
        return id;
    };
}
                
let filenameGenerator = startGenerator();

export function genFreshFileName (x) {
    return `${x}.${filenameGenerator()}`;
}

let functionNameGenerator = startGenerator();

export function genGlobalFunctionName (x, filename) {
    let prefix = filename ? `__ejs[${filename}]` : "__ejs_fn";
    return `${prefix}_${x}_${functionNameGenerator()}`;
}

export function genAnonymousFunctionName (filename) {
    let prefix = filename ? `__ejs[${filename}]_%anon` : "__ejs_%anon";
    return `${prefix}_${functionNameGenerator()}`;
}

export function bold () {
/*
    if (process && process.stderr && process.stderr.isTTY)
	return terminal.ANSIStyle("bold");
*/
    return "";
}
                
export function reset () {
/*
    if (process && process.stderr && process.stderr.isTTY)
        return terminal.ANSIStyle("reset");
*/
    return "";
}

export function is_number_literal(n) { return n.type === Literal && typeof(n.value) === "number"; }
export function is_string_literal(n) { return n.type === Literal && typeof(n.raw) === "string"; }
export function create_intrinsic (id, args, loc) {
    return {
        type: CallExpression,
        callee: id,
        arguments: args,
        loc: loc
    };
}

export function is_intrinsic (n, name) {
    if (n.type !== CallExpression) return false;
    if (n.callee.type !== Identifier) return false;
    if (n.callee.name[0] !== '%') return false;
    if (name && n.callee.name !== name) return false;

    return true;
}


export function intrinsic (id, args, loc) {
    let rv = b.callExpression(id, args);
    rv.loc = loc;
    return rv;
}
