/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// SSA construction (Braun et al., "Simple and Efficient Construction of
// Static Single Assignment Form"):
//   - readVariable/writeVariable give the AST lowering a mutable-variable
//     view; block parameters materialize on demand at joins.
//   - blocks are sealed once all predecessors are known; incomplete
//     parameters get their per-edge arguments filled in.
//   - trivial parameters (all incoming arguments equal, or only the
//     parameter itself) are removed recursively.

import { Func, Block, Inst, replaceAllUses, usersOf } from "./ir";
import type { Imms } from "./ir";
import { opInfo, Effect } from "./ops";

export class FunctionBuilder {
    fn: Func;
    // varname -> (block -> value)
    defs = new Map<string, Map<Block, Inst>>();
    cur!: Block;
    // stack of catch blocks; when non-empty, may-throw instructions get
    // explicit normal/unwind edges (invoke style)
    handlers: Block[] = [];

    constructor(name: string, paramNames: string[]) {
        this.fn = new Func(name, paramNames);

        const entry = this.newBlock("entry");
        this.setInsertPoint(entry);
        // function parameters are the entry block's parameters
        for (const pname of this.fn.paramNames) {
            const p = entry.addParam(pname);
            this.writeVariable(pname, entry, p);
        }
        this.sealBlock(entry);
    }

    newBlock(name?: string): Block {
        return this.fn.addBlock(new Block(this.fn, name));
    }

    setInsertPoint(block: Block): void {
        this.cur = block;
    }

    // --- instruction emission ------------------------------------------------

    emit(op: string, operands: Inst[], imms: Imms): Inst {
        if (this.cur.terminated)
            throw new Error(`emitting '${op}' into terminated block ${this.cur.name}`);
        const inst = new Inst(this.fn, op, operands, imms);
        inst.block = this.cur;
        this.cur.insts.push(inst);

        // inside a protected region, a may-throw instruction terminates its
        // block with an explicit normal/unwind pair, and insertion continues
        // in the normal successor.
        const info = opInfo(op);
        if (this.handlers.length > 0 && (info.effects & Effect.THROW) !== 0 && !info.terminator) {
            const handler = this.handlers[this.handlers.length - 1]!;
            const cont = this.newBlock("cont");
            inst.addTarget(cont, [], "normal");
            inst.addTarget(handler, [], "unwind");
            this.sealBlock(cont);
            this.setInsertPoint(cont);
        }
        return inst;
    }

    // --- exception handling -----------------------------------------------------

    newCatchBlock(name?: string): Block {
        const block = this.newBlock(name || "catch");
        block.isCatch = true;
        const exc = block.addParam("%exception");
        exc.isException = true;
        exc.type = "exception";
        return block;
    }

    pushHandler(catchBlock: Block): void {
        this.handlers.push(catchBlock);
    }

    popHandler(): Block | undefined {
        return this.handlers.pop();
    }

    // a `throw` statement: unwinds to the active handler if there is one,
    // otherwise out of the function.
    throwValue(v: Inst): Inst {
        const inst = this.emit("throw", [v], {});
        if (this.handlers.length > 0)
            inst.addTarget(this.handlers[this.handlers.length - 1]!, [], "unwind");
        return inst;
    }

    constNumber(v: number): Inst {
        return this.emit("const", [], { kind: "number", value: v });
    }
    constAtom(s: string): Inst {
        return this.emit("const", [], { kind: "atom", value: s });
    }
    constBool(v: boolean): Inst {
        return this.emit("const", [], { kind: "boolean", value: v });
    }
    constUndefined(): Inst {
        return this.emit("const", [], { kind: "undefined" });
    }
    constNull(): Inst {
        return this.emit("const", [], { kind: "null" });
    }

    br(block: Block, args?: Inst[]): Inst {
        const inst = this.emit("br", [], {});
        inst.addTarget(block, args || []);
        return inst;
    }

    condBr(cond: Inst, tblock: Block, targs: Inst[], fblock: Block, fargs: Inst[]): Inst {
        const inst = this.emit("cond_br", [cond], {});
        inst.addTarget(tblock, targs || []);
        inst.addTarget(fblock, fargs || []);
        return inst;
    }

    ret(value: Inst): Inst {
        return this.emit("return", [value], {});
    }

    // --- Braun SSA -------------------------------------------------------------

    writeVariable(name: string, block: Block, value: Inst): void {
        let m = this.defs.get(name);
        if (!m) {
            m = new Map();
            this.defs.set(name, m);
        }
        m.set(block, value);
    }

    hasVariable(name: string): boolean {
        return this.defs.has(name);
    }

    readVariable(name: string, block: Block): Inst {
        const m = this.defs.get(name);
        const v = m && m.get(block);
        if (v) return v;
        return this.readVariableRecursive(name, block);
    }

    readVariableRecursive(name: string, block: Block): Inst {
        let val: Inst;
        if (!block.sealed) {
            // incomplete CFG: leave a parameter to be filled at seal time
            const param = block.addParam(name);
            block.incompleteParams.set(name, param);
            val = param;
        } else if (block.predEdges.length === 1) {
            val = this.readVariable(name, block.predEdges[0]!.inst.block!);
        } else if (block.predEdges.length === 0) {
            if (block !== this.fn.entry) {
                // an unreachable block (code after `while (true)`, after a
                // switch whose every case returns, ...): any value will do.
                // the emitter drops unreachable blocks entirely.
                const c = new Inst(this.fn, "const", [], { kind: "undefined" });
                c.block = block;
                block.insts.unshift(c);
                val = c;
            } else {
                throw new Error(`EIR: read of undefined variable '${name}' reached entry`);
            }
        } else {
            // break potential cycles with a parameter before recursing
            const param = block.addParam(name);
            this.writeVariable(name, block, param);
            val = this.addParamOperands(name, param);
        }
        this.writeVariable(name, block, val);
        return val;
    }

    addParamOperands(name: string, param: Inst): Inst {
        const block = param.block!;
        const argIdx = block.argIndexOfParam(param);
        for (const e of block.predEdges) {
            const predBlock = e.inst.block!;
            const v = this.readVariable(name, predBlock);
            e.inst.targets![e.targetIndex]!.args[argIdx] = v;
        }
        return this.tryRemoveTrivialParam(param);
    }

    tryRemoveTrivialParam(param: Inst): Inst {
        if (param.isException) return param; // produced by unwinding, never trivial
        const block = param.block!;
        const argIdx = block.argIndexOfParam(param);
        let same: Inst | null = null;
        for (const e of block.predEdges) {
            const arg = e.inst.targets![e.targetIndex]!.args[argIdx];
            if (arg === same || arg === param) continue;
            if (same !== null) return param; // merges at least two distinct values: keep it
            same = arg ?? null;
        }
        // unreachable block or self-reference only
        if (same === null) return param;

        // collect users before rewriting so we can recheck dependent params
        const users = usersOf(this.fn, param).filter((u) => u !== param);

        replaceAllUses(this.fn, param, same);
        // fix stale variable definitions that still point at the removed param
        for (const m of this.defs.values()) {
            for (const entry of m.entries()) {
                if (entry[1] === param) m.set(entry[0], same);
            }
        }
        block.removeParam(param);

        for (const u of users) {
            if (u.op === "blockparam" && !u.removed) this.tryRemoveTrivialParam(u);
        }
        return same;
    }

    sealBlock(block: Block): void {
        if (block.sealed) throw new Error(`sealing already-sealed block ${block.name}`);
        block.sealed = true;
        for (const entry of block.incompleteParams.entries()) {
            this.addParamOperands(entry[0], entry[1]);
        }
        block.incompleteParams.clear();
    }

    finish(): Func {
        for (const b of this.fn.blocks) {
            if (!b.sealed) throw new Error(`EIR: ${this.fn.name}: block ${b.name} never sealed`);
        }
        return this.fn;
    }
}
