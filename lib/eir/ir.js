/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

// EIR core data structures: Module / Function / Block / Inst.
// SSA with basic block arguments (no phi nodes); block parameters are
// Insts with op "blockparam".  See EIRProposal.md.

import { opInfo, isTerminator } from "./ops";

export class Module {
    constructor(name) {
        this.name = name;
        this.functions = [];
    }

    addFunction(fn) {
        this.functions.push(fn);
        return fn;
    }
}

export class Func {
    constructor(name, paramNames) {
        this.name = name;
        this.paramNames = paramNames || [];
        this.blocks = [];
        this.next_value_id = 0;
        this.next_block_id = 0;
        this.entry = null;
    }

    newValueId() {
        return this.next_value_id++;
    }

    addBlock(block) {
        this.blocks.push(block);
        if (!this.entry) this.entry = block;
        return block;
    }

    // all instructions, params first per block, in block order.
    forEachInst(cb) {
        for (let b of this.blocks) {
            for (let p of b.params) cb(p, b);
            for (let i of b.insts) cb(i, b);
        }
    }
}

export class Block {
    constructor(fn, name) {
        this.fn = fn;
        // uniquify within the function so lowering can reuse friendly names
        this.name = `${name || "bb"}${fn.next_block_id++}`;
        this.params = [];
        this.insts = [];
        this.sealed = false;
        // catch blocks are reached only by unwind edges; their first param
        // is the caught exception, produced by the unwind machinery rather
        // than passed as an edge argument.
        this.isCatch = false;
        // predecessor edges: { inst: <terminator>, targetIndex: <index into inst.targets> }
        this.predEdges = [];
        // Braun SSA construction state (owned by the builder)
        this.incompleteParams = new Map(); // varname -> param Inst
    }

    // edge args don't carry the exception param, so a param's position in
    // an edge's args differs from its position in `params` on catch blocks.
    argIndexOfParam(param) {
        return param.paramIndex - (this.isCatch ? 1 : 0);
    }

    get terminator() {
        let last = this.insts[this.insts.length - 1];
        if (last && isTerminator(last)) return last;
        return null;
    }

    get terminated() {
        return this.terminator !== null;
    }

    preds() {
        return this.predEdges.map((e) => e.inst.block);
    }

    succs() {
        let t = this.terminator;
        if (!t || !t.targets) return [];
        return t.targets.map((tgt) => tgt.block);
    }

    addParam(nameHint) {
        let p = new Inst(this.fn, "blockparam", [], {});
        p.block = this;
        p.nameHint = nameHint;
        p.paramIndex = this.params.length;
        this.params.push(p);
        // extend every known predecessor edge with a slot for this param.
        // callers (the builder) fill the values in.
        if (!p.isException) {
            for (let e of this.predEdges) {
                e.inst.targets[e.targetIndex].args.push(null);
            }
        }
        return p;
    }

    removeParam(param) {
        let idx = param.paramIndex;
        let argIdx = this.argIndexOfParam(param);
        this.params.splice(idx, 1);
        for (let i = idx; i < this.params.length; i++) this.params[i].paramIndex = i;
        for (let e of this.predEdges) {
            e.inst.targets[e.targetIndex].args.splice(argIdx, 1);
        }
        param.removed = true;
    }
}

export class Inst {
    // operands: array of Inst (values); imms: object of immediates
    constructor(fn, op, operands, imms) {
        this.id = fn.newValueId();
        this.op = op;
        this.operands = operands || [];
        this.imms = imms || {};
        this.block = null;
        this.type = "any";
        // control-flow targets for terminators / invokes:
        // [{ block, args: [Inst], kind: "normal"|"unwind"|undefined }]
        this.targets = null;

        let info = opInfo(op);
        if (info.arity >= 0 && this.operands.length !== info.arity)
            throw new Error(
                `EIR: '${op}' expects ${info.arity} operands, got ${this.operands.length}`
            );
    }

    addTarget(block, args, kind) {
        if (!this.targets) this.targets = [];
        let targetIndex = this.targets.length;
        this.targets.push({ block: block, args: args || [], kind: kind });
        block.predEdges.push({ inst: this, targetIndex: targetIndex });
    }
}

// replace every use of `from` (as an operand or edge argument) in fn with `to`.
export function replaceAllUses(fn, from, to) {
    fn.forEachInst((inst) => {
        for (let i = 0; i < inst.operands.length; i++) {
            if (inst.operands[i] === from) inst.operands[i] = to;
        }
        if (inst.targets) {
            for (let t of inst.targets) {
                for (let i = 0; i < t.args.length; i++) {
                    if (t.args[i] === from) t.args[i] = to;
                }
            }
        }
    });
}

// collect the instructions that use `value` (operands or edge args).
export function usersOf(fn, value) {
    let users = [];
    fn.forEachInst((inst) => {
        let uses = false;
        for (let o of inst.operands) if (o === value) uses = true;
        if (inst.targets) {
            for (let t of inst.targets) for (let a of t.args) if (a === value) uses = true;
        }
        if (uses) users.push(inst);
    });
    return users;
}
