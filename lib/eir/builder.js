/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

// EIR function builder with on-the-fly SSA construction.
//
// This implements Braun, Buchwald, Hack et al., "Simple and Efficient
// Construction of Static Single Assignment Form" (CC 2013), adapted to
// basic-block arguments instead of phis:
//
//   - writeVariable/readVariable give the AST lowering a mutable-variable
//     view; the builder inserts block parameters at join points on demand.
//   - blocks start unsealed; sealing a block promises no further
//     predecessors will be added, at which point pending ("incomplete")
//     parameters get their per-edge arguments filled in.
//   - trivial parameters (all incoming arguments equal, or only the
//     parameter itself) are removed recursively.

import { Func, Block, Inst, replaceAllUses, usersOf } from "./ir";
import { opInfo } from "./ops";

export class FunctionBuilder {
    constructor(name, paramNames) {
        this.fn = new Func(name, paramNames);
        // varname -> (block -> value)
        this.defs = new Map();
        this.cur = null;

        let entry = this.newBlock("entry");
        this.setInsertPoint(entry);
        // function parameters are the entry block's parameters
        for (let pname of this.fn.paramNames) {
            let p = entry.addParam(pname);
            this.writeVariable(pname, entry, p);
        }
        this.sealBlock(entry);
    }

    newBlock(name) {
        return this.fn.addBlock(new Block(this.fn, name));
    }

    setInsertPoint(block) {
        this.cur = block;
    }

    // --- instruction emission ------------------------------------------------

    emit(op, operands, imms) {
        if (this.cur.terminated) throw new Error(`emitting '${op}' into terminated block ${this.cur.name}`);
        let inst = new Inst(this.fn, op, operands, imms);
        inst.block = this.cur;
        this.cur.insts.push(inst);
        return inst;
    }

    constNumber(v) {
        return this.emit("const", [], { kind: "number", value: v });
    }
    constAtom(s) {
        return this.emit("const", [], { kind: "atom", value: s });
    }
    constBool(v) {
        return this.emit("const", [], { kind: "boolean", value: v });
    }
    constUndefined() {
        return this.emit("const", [], { kind: "undefined" });
    }
    constNull() {
        return this.emit("const", [], { kind: "null" });
    }

    br(block, args) {
        let inst = this.emit("br", [], {});
        inst.addTarget(block, args || []);
        return inst;
    }

    condBr(cond, tblock, targs, fblock, fargs) {
        let inst = this.emit("cond_br", [cond], {});
        inst.addTarget(tblock, targs || []);
        inst.addTarget(fblock, fargs || []);
        return inst;
    }

    ret(value) {
        return this.emit("return", [value], {});
    }

    // --- Braun SSA -------------------------------------------------------------

    writeVariable(name, block, value) {
        let m = this.defs.get(name);
        if (!m) {
            m = new Map();
            this.defs.set(name, m);
        }
        m.set(block, value);
    }

    hasVariable(name) {
        return this.defs.has(name);
    }

    readVariable(name, block) {
        let m = this.defs.get(name);
        if (m && m.has(block)) return m.get(block);
        return this.readVariableRecursive(name, block);
    }

    readVariableRecursive(name, block) {
        let val;
        if (!block.sealed) {
            // incomplete CFG: leave a parameter to be filled at seal time
            let param = block.addParam(name);
            block.incompleteParams.set(name, param);
            val = param;
        } else if (block.predEdges.length === 1) {
            val = this.readVariable(name, block.predEdges[0].inst.block);
        } else if (block.predEdges.length === 0) {
            throw new Error(`EIR: read of undefined variable '${name}' reached entry`);
        } else {
            // break potential cycles with a parameter before recursing
            let param = block.addParam(name);
            this.writeVariable(name, block, param);
            val = this.addParamOperands(name, param);
        }
        this.writeVariable(name, block, val);
        return val;
    }

    addParamOperands(name, param) {
        let block = param.block;
        for (let e of block.predEdges) {
            let predBlock = e.inst.block;
            let v = this.readVariable(name, predBlock);
            e.inst.targets[e.targetIndex].args[param.paramIndex] = v;
        }
        return this.tryRemoveTrivialParam(param);
    }

    tryRemoveTrivialParam(param) {
        let block = param.block;
        let same = null;
        for (let e of block.predEdges) {
            let arg = e.inst.targets[e.targetIndex].args[param.paramIndex];
            if (arg === same || arg === param) continue;
            if (same !== null) return param; // merges at least two distinct values: keep it
            same = arg;
        }
        // unreachable block or self-reference only
        if (same === null) return param;

        // collect users before rewriting so we can recheck dependent params
        let users = usersOf(this.fn, param).filter((u) => u !== param);

        replaceAllUses(this.fn, param, same);
        // fix stale variable definitions that still point at the removed param
        for (let m of this.defs.values()) {
            for (let entry of m.entries()) {
                if (entry[1] === param) m.set(entry[0], same);
            }
        }
        block.removeParam(param);

        for (let u of users) {
            if (u.op === "blockparam" && !u.removed) this.tryRemoveTrivialParam(u);
        }
        return same;
    }

    sealBlock(block) {
        if (block.sealed) throw new Error(`sealing already-sealed block ${block.name}`);
        block.sealed = true;
        for (let entry of block.incompleteParams.entries()) {
            this.addParamOperands(entry[0], entry[1]);
        }
        block.incompleteParams.clear();
    }

    finish() {
        for (let b of this.fn.blocks) {
            if (!b.sealed) throw new Error(`EIR: block ${b.name} never sealed`);
        }
        return this.fn;
    }
}
