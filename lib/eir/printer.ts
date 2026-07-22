/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// canonical textual form of EIR.  deterministic (values numbered densely in
// print order) so it can back golden tests.

import { opInfo } from "./ops";
import type { Func, Inst, Module, Target, ImmValue } from "./ir";

function fmtImm(v: ImmValue): string {
    if (typeof v === "string") return JSON.stringify(v);
    if (Array.isArray(v)) return `[${v.map(fmtImm).join(", ")}]`;
    return String(v);
}

type NameOf = (v: Inst | null | undefined) => string;

export function printFunction(fn: Func): string {
    // dense renumbering in block/instruction order for stable output
    const names = new Map<Inst, string>();
    let next = 0;
    const nameOf: NameOf = (v) => {
        if (v === null || v === undefined) return "<null>";
        let name = names.get(v);
        if (name === undefined) {
            name = `%${next++}`;
            names.set(v, name);
        }
        return name;
    };

    for (const b of fn.blocks) {
        for (const p of b.params) nameOf(p);
        for (const i of b.insts) nameOf(i);
    }

    const lines: string[] = [];
    const header_params = fn.entry ? fn.entry.params.map((p) => `${nameOf(p)}: ${p.type}`) : [];
    lines.push(`fn @${fn.name}(${header_params.join(", ")}) {`);

    const paramStr = (p: Inst) => `${nameOf(p)}: ${p.type}`;

    for (const b of fn.blocks) {
        if (b === fn.entry) {
            lines.push(`^${b.name}:`);
        } else {
            const inner = b.params.map(paramStr).join(", ");
            lines.push(`^${b.name}(${inner}):`);
        }

        for (const inst of b.insts) {
            lines.push(`    ${printInst(inst, nameOf)}`);
        }
    }
    lines.push("}");
    return lines.join("\n");
}

function printTarget(t: Target, nameOf: NameOf): string {
    const args = t.args.map((a) => nameOf(a)).join(", ");
    const kind = t.kind ? `${t.kind} ` : "";
    return `${kind}^${t.block.name}(${args})`;
}

export function printInst(inst: Inst, nameOf: NameOf): string {
    const info = opInfo(inst.op);

    const producesValue =
        inst.op !== "br" &&
        inst.op !== "cond_br" &&
        inst.op !== "return" &&
        inst.op !== "throw" &&
        inst.op !== "unreachable";

    const operand_strs = inst.operands.map((o) => nameOf(o));
    const imm_strs: string[] = [];
    if (info.imms) {
        for (const imm of info.imms) {
            if (inst.imms[imm] !== undefined) imm_strs.push(`${imm}=${fmtImm(inst.imms[imm])}`);
        }
    }

    const all = operand_strs.concat(imm_strs);
    let text = inst.op + (all.length ? " " + all.join(", ") : "");

    if (inst.targets && inst.targets.length > 0) {
        text += " -> " + inst.targets.map((t) => printTarget(t, nameOf)).join(", ");
    }

    // typed defs (the low tier) print their type; "any" stays bare so all
    // existing output is byte-identical
    if (producesValue)
        return inst.type === "any"
            ? `${nameOf(inst)} = ${text}`
            : `${nameOf(inst)}: ${inst.type} = ${text}`;
    return text;
}

export function printModule(mod: Module): string {
    const out = [`module ${mod.name} {`];
    for (const fn of mod.functions) {
        out.push(printFunction(fn));
        out.push("");
    }
    out.push("}");
    return out.join("\n");
}
