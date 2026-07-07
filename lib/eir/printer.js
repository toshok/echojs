/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

// canonical textual form of EIR.  deterministic (values numbered densely in
// print order) so it can back golden tests.

import { opInfo, isTerminator } from "./ops";

function fmtImm(v) {
    if (typeof v === "string") return JSON.stringify(v);
    if (Array.isArray(v)) return `[${v.map(fmtImm).join(", ")}]`;
    return String(v);
}

export function printFunction(fn) {
    // dense renumbering in block/instruction order for stable output
    let names = new Map();
    let next = 0;
    let nameOf = (v) => {
        if (v === null || v === undefined) return "<null>";
        if (!names.has(v)) names.set(v, `%${next++}`);
        return names.get(v);
    };

    for (let b of fn.blocks) {
        for (let p of b.params) nameOf(p);
        for (let i of b.insts) nameOf(i);
    }

    let lines = [];
    let header_params = fn.entry ? fn.entry.params.map((p) => `${nameOf(p)}: ${p.type}`) : [];
    lines.push(`fn @${fn.name}(${header_params.join(", ")}) {`);

    for (let b of fn.blocks) {
        let plist = "";
        if (b !== fn.entry && b.params.length > 0)
            plist = `(${b.params.map((p) => `${nameOf(p)}: ${p.type}`).join(", ")})`;
        else if (b !== fn.entry) plist = "()";
        if (b === fn.entry) lines.push(`^${b.name}:`);
        else lines.push(`^${b.name}${plist}:`);

        for (let inst of b.insts) {
            lines.push(`    ${printInst(inst, nameOf)}`);
        }
    }
    lines.push("}");
    return lines.join("\n");
}

function printTarget(t, nameOf) {
    let args = t.args.map((a) => nameOf(a)).join(", ");
    let kind = t.kind ? `${t.kind} ` : "";
    return `${kind}^${t.block.name}(${args})`;
}

export function printInst(inst, nameOf) {
    let info = opInfo(inst.op);
    let parts = [];

    let producesValue = !info.terminator || false;
    // terminators don't produce values; invoke-style calls do
    if (inst.op === "br" || inst.op === "cond_br" || inst.op === "return" ||
        inst.op === "throw" || inst.op === "unreachable")
        producesValue = false;
    else producesValue = true;

    let rhs = [inst.op];

    let operand_strs = inst.operands.map((o) => nameOf(o));
    let imm_strs = [];
    if (info.imms) {
        for (let imm of info.imms) {
            if (inst.imms[imm] !== undefined) imm_strs.push(`${imm}=${fmtImm(inst.imms[imm])}`);
        }
    }

    let all = operand_strs.concat(imm_strs);
    let text = inst.op + (all.length ? " " + all.join(", ") : "");

    if (inst.targets && inst.targets.length > 0) {
        text += " -> " + inst.targets.map((t) => printTarget(t, nameOf)).join(", ");
    }

    if (producesValue) return `${nameOf(inst)} = ${text}`;
    return text;
}

export function printModule(mod) {
    let out = [`module ${mod.name} {`];
    for (let fn of mod.functions) {
        out.push(printFunction(fn));
        out.push("");
    }
    out.push("}");
    return out.join("\n");
}
