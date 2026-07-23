/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// EIR core data structures: Module / Func / Block / Inst.
// SSA with basic block arguments (no phi nodes); block parameters are
// Insts with op "blockparam".  See EIRProposal.md.

import { opInfo, isTerminator } from "./ops";

// the immediate (non-value) attributes an instruction carries.  values
// are op-specific: atoms and runtime-fn names are strings, env slots and
// array lengths are numbers, make_object keys / make_array indices are
// arrays, template_callsite carries string arrays, etc.
export type ImmValue =
    | string
    | number
    | boolean
    | null
    | undefined
    | readonly string[]
    | readonly number[];

export type Imms = { [name: string]: ImmValue };

export type TargetKind = "normal" | "unwind" | undefined;

export interface Target {
    block: Block;
    args: (Inst | null)[];
    kind: TargetKind;
}

export interface PredEdge {
    inst: Inst;
    targetIndex: number;
}

export class Module {
    name: string;
    functions: Func[] = [];

    constructor(name: string) {
        this.name = name;
    }

    addFunction(fn: Func): Func {
        this.functions.push(fn);
        return fn;
    }
}

export class Func {
    name: string;
    paramNames: string[];
    blocks: Block[] = [];
    next_value_id = 0;
    next_block_id = 0;
    entry: Block | null = null;

    constructor(name: string, paramNames?: string[]) {
        this.name = name;
        this.paramNames = paramNames || [];
    }

    newValueId(): number {
        return this.next_value_id++;
    }

    addBlock(block: Block): Block {
        this.blocks.push(block);
        if (!this.entry) this.entry = block;
        return block;
    }

    // all instructions, params first per block, in block order.
    forEachInst(cb: (inst: Inst, block: Block) => void): void {
        for (const b of this.blocks) {
            for (const p of b.params) cb(p, b);
            for (const i of b.insts) cb(i, b);
        }
    }
}

export class Block {
    fn: Func;
    // uniquified within the function so lowering can reuse friendly names
    name: string;
    params: Inst[] = [];
    insts: Inst[] = [];
    sealed = false;
    // catch blocks are reached only by unwind edges; their first param
    // is the caught exception, produced by the unwind machinery rather
    // than passed as an edge argument.
    isCatch = false;
    // predecessor edges
    predEdges: PredEdge[] = [];
    // Braun SSA construction state (owned by the builder):
    // varname -> param Inst
    incompleteParams = new Map<string, Inst>();

    constructor(fn: Func, name?: string) {
        this.fn = fn;
        this.name = `${name || "bb"}${fn.next_block_id++}`;
    }

    // edge args don't carry the exception param, so a param's position in
    // an edge's args differs from its position in `params` on catch blocks.
    argIndexOfParam(param: Inst): number {
        return param.paramIndex - (this.isCatch ? 1 : 0);
    }

    get terminator(): Inst | null {
        const last = this.insts[this.insts.length - 1];
        if (last && isTerminator(last)) return last;
        return null;
    }

    get terminated(): boolean {
        return this.terminator !== null;
    }

    preds(): Block[] {
        return this.predEdges.map((e) => e.inst.block!);
    }

    succs(): Block[] {
        const t = this.terminator;
        if (!t || !t.targets) return [];
        return t.targets.map((tgt) => tgt.block);
    }

    addParam(nameHint?: string): Inst {
        const p = new Inst(this.fn, "blockparam", [], {});
        p.block = this;
        p.nameHint = nameHint;
        p.paramIndex = this.params.length;
        this.params.push(p);
        // extend every known predecessor edge with a slot for this param.
        // callers (the builder) fill the values in.
        if (!p.isException) {
            for (const e of this.predEdges) {
                e.inst.targets![e.targetIndex]!.args.push(null);
            }
        }
        return p;
    }

    removeParam(param: Inst): void {
        const idx = param.paramIndex;
        const argIdx = this.argIndexOfParam(param);
        this.params.splice(idx, 1);
        for (let i = idx; i < this.params.length; i++) this.params[i]!.paramIndex = i;
        for (const e of this.predEdges) {
            e.inst.targets![e.targetIndex]!.args.splice(argIdx, 1);
        }
        param.removed = true;
    }
}

export class Inst {
    id: number;
    op: string;
    // operand values.  slots are filled by construction and never null in
    // a verified function; the builder's edge machinery temporarily holds
    // nulls in Target.args only.
    operands: Inst[];
    imms: Imms;
    block: Block | null = null;
    type = "any"; // the (future) type lattice; untyped for now
    // control-flow targets for terminators / invokes
    targets: Target[] | null = null;

    // --- blockparam bookkeeping ------------------------------------------------
    nameHint: string | undefined = undefined;
    paramIndex = -1;
    // catch blocks' first param is the caught exception
    isException = false;
    removed = false;
    // Phase 3.4 pass (b): a block parameter that carries a RAW f64 across
    // its incoming edges — the controlled lift of the Phase 2
    // raw-values-cannot-cross-blocks rule.  Set ONLY by the optimizer's
    // guard-region merge (optimize-guards.ts) on joins it builds/rewires;
    // lowering must never set it, so every lowering-created edge keeps
    // the strict boxed rule.  The marker is not trusted on its own: the
    // verifier independently checks the full safety conditions (type is
    // f64, every incoming argument is f64, non-catch block, no unwind
    // edges), so a stray marker can only ever *tighten* checking, never
    // admit an ill-typed edge.  The emitter types the phi as double.
    rawJoin = false;

    constructor(fn: Func, op: string, operands?: Inst[], imms?: Imms) {
        this.id = fn.newValueId();
        this.op = op;
        this.operands = operands || [];
        this.imms = imms || {};

        const info = opInfo(op);
        if (info.arity >= 0 && this.operands.length !== info.arity)
            throw new Error(
                `EIR: '${op}' expects ${info.arity} operands, got ${this.operands.length}`
            );
        if (info.sig) this.type = info.sig.result; // the low tier's typed results
    }

    addTarget(block: Block, args?: (Inst | null)[], kind?: TargetKind): void {
        if (!this.targets) this.targets = [];
        const targetIndex = this.targets.length;
        this.targets.push({ block, args: args || [], kind });
        block.predEdges.push({ inst: this, targetIndex });
    }
}

// replace every use of `from` (as an operand or edge argument) in fn with `to`.
export function replaceAllUses(fn: Func, from: Inst, to: Inst): void {
    fn.forEachInst((inst) => {
        for (let i = 0; i < inst.operands.length; i++) {
            if (inst.operands[i] === from) inst.operands[i] = to;
        }
        if (inst.targets) {
            for (const t of inst.targets) {
                for (let i = 0; i < t.args.length; i++) {
                    if (t.args[i] === from) t.args[i] = to;
                }
            }
        }
    });
}

// collect the instructions that use `value` (operands or edge args).
export function usersOf(fn: Func, value: Inst): Inst[] {
    const users: Inst[] = [];
    fn.forEachInst((inst) => {
        let uses = false;
        for (const o of inst.operands) if (o === value) uses = true;
        if (inst.targets) {
            for (const t of inst.targets) for (const a of t.args) if (a === value) uses = true;
        }
        if (uses) users.push(inst);
    });
    return users;
}
