/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

// AST -> EIR lowering.
//
// Covers a whitelisted subset of the (desugared) AST; anything else throws
// LowerNotSupported so callers can fall back to the legacy LLVMIRVisitor
// per-function.  The subset grows until nothing falls back.
//
// Scope resolution (lib/eir/scopes.js) runs first and decides, per binding:
// SSA local vs. environment slot.  Lowering then emits make_env /
// env_load / env_store / make_closure directly — this replaces new-cc for
// the EIR path.
//
// Calling convention mirrors the runtime: every function takes
// (%env, %this, ...params).
//
// Handled: literals (incl. regex), identifiers (locals/captured/globals),
// var/let/const, assignment (= and compound), update (++/--),
// binary/logical/unary operators, member access, calls, new, this,
// sequence/array/object literals, untagged template literals, function
// declarations and expressions, arrow functions (full closure support,
// lexical `this` via the owner's captured this binding), default/rest
// parameters, `arguments`, if/else, while, do-while, for, for-of,
// for-in, switch, break/continue, return, throw, try/catch (unwind
// edges), try/finally (finalizer duplication), per-iteration loop
// environments, and the %-intrinsic calls listed in intrinsics.js
// (produced by the pre-EIR desugar passes, e.g. %arrayFromSpread).
//
// Not yet: tagged templates, `this` in a candidate whose root is itself
// an arrow (needs the module toplevel's this).

import * as b from "../ast-builder";
import { FunctionBuilder } from "./builder";
import { Module } from "./ir";
import { ScopeAnalysis, compound_assign_ops } from "./scopes";
import { LowerNotSupported, isLowerNotSupported } from "./errors";
import { eir_intrinsics } from "./intrinsics";

const binops = {
    "+": "add",
    "-": "sub",
    "*": "mul",
    "/": "div",
    "%": "mod",
    "<": "lt",
    "<=": "le",
    ">": "gt",
    ">=": "ge",
    "==": "loose_eq",
    "!=": "loose_neq",
    "===": "strict_eq",
    "!==": "strict_neq",
    "&": "bitand",
    "|": "bitor",
    "^": "bitxor",
    "<<": "shl",
    ">>": "shr",
    ">>>": "ushr",
    instanceof: "instanceof",
    in: "in",
};

// the source-level name a closure should carry (Function.prototype.name):
// the function's own id, or "" for anonymous functions — never the
// scope-qualified EIR name
function displayNameOf(childInfo) {
    return (childInfo.node.id && childInfo.node.id.name) || "";
}

class LowerFunction {
    constructor(info, analysis, module, mod_ctx) {
        this.info = info; // FnInfo from scope analysis
        this.analysis = analysis;
        this.module = module;
        // toplevel-as-EIR: this function IS the module toplevel; import/
        // export statements lower here, and slot-backed declarations store
        // through module slots instead of local bindings
        this.isToplevel = !!info.isToplevel;
        // module-scope interop: module-slot references (imports and this
        // module's exports: name -> {module, slot, constval?, writable})
        // and sibling top-level EIR functions callable directly
        this.mod_ctx = mod_ctx || { refs: new Map(), siblings: new Map() };

        let paramNames = info.params.map((p) => p.uid);
        this.b = new FunctionBuilder(info.name, ["%env", "%this"].concat(paramNames));
        this.envParam = this.b.fn.entry.params[0];
        this.thisParam = this.b.fn.entry.params[1];
        // `this` reads go through the builder variable "%this" (seeded to
        // the entry param by the builder): a derived constructor's super()
        // call rebinds it (the runtime constructs the object and returns
        // it), and SSA carries the update.  for every other function it
        // collapses to the entry param.

        // break/continue targets.  loops push onto both stacks; switch
        // statements only onto breakTargets (continue passes through a
        // switch to the enclosing loop).
        this.breakTargets = [];
        this.continueTargets = [];
        // labeled targets: LabeledStatement pushes loop labels onto
        // pendingLabels; the loop lowering claims them (activeLabels)
        // against its own exit/continue blocks.  non-loop labels get a
        // synthetic exit block.  ctxLen = finallyCtx.length at label
        // entry, so a labeled exit runs exactly the finalizers entered
        // since the label.
        this.pendingLabels = [];
        this.activeLabels = [];
        // materialized per-iteration loop envs lexically active at the
        // current lowering position (innermost last).  the current env
        // value of each is tracked as a builder variable ("%loopenv#id"),
        // so per-iteration refreshes flow through SSA/block params like
        // any other variable (envs are ejsvals).
        this.activeLoopEnvs = [];
        // active try/finally contexts.  abrupt exits (return, break,
        // continue) crossing a finally boundary lower a fresh copy of each
        // crossed finalizer at the exit site (finalizer duplication).
        this.finallyCtx = [];

        // environment setup
        this.curEnv = this.envParam;
        if (info.envSize > 0) {
            this.curEnv = this.b.emit("make_env", [], { size: info.envSize });
            if (info.parentSlot >= 0)
                this.b.emit("env_store", [this.curEnv, this.envParam], {
                    slot: info.parentSlot,
                });
            // captured parameters live in the env from function entry
            for (let p of info.params) {
                if (p.captured) {
                    let v = this.b.readVariable(p.uid, this.b.cur);
                    this.b.emit("env_store", [this.curEnv, v], { slot: p.slot });
                }
            }
        }

        // hoisted-var semantics: every local is readable (as undefined)
        // from function entry, even before its declaration statement runs
        // (`use(x); ... if (c) { var x = 5; }`).  the declaration-time
        // write in VariableDeclaration still handles the per-iteration
        // reset of block-scoped lets in loops.  loop-env bindings are
        // skipped: their env doesn't exist yet (it's created at loop
        // entry), and being let/const they're only visible inside the loop.
        for (let binding of info.bindings) {
            if (binding.loopEnv && binding.loopEnv.materialized) continue;
            if (binding.kind === "local") this.writeBinding(binding, this.b.constUndefined());
        }

        // the arguments object, if referenced anywhere in this function
        if (info.usesArguments) {
            let a = this.b.emit("args_obj", [], {});
            this.writeBinding(info.argumentsBinding, a);
        }

        // an arrow below captures our `this`: store it in the env (kept
        // in sync by intrinsicCall when super() rebinds this)
        if (info.thisBinding && info.thisBinding.captured)
            this.writeBinding(info.thisBinding, this.thisParam);

        // the rest parameter materializes from the trailing arguments
        if (info.restBinding) {
            let rest = this.b.emit("rest_args", [], { index: info.params.length });
            this.writeBinding(info.restBinding, rest);
        }

        // default parameters: a param that arrived undefined takes its
        // default (evaluated left to right, in the function scope).  the
        // conditional write merges via SSA (or the env, for captured
        // params, whose initial store just happened above).
        let defaults = info.defaults || [];
        let ndefaults = Math.min(defaults.length, info.params.length);
        for (let i = 0; i < ndefaults; i++) {
            if (!defaults[i]) continue;
            let pb = info.params[i];
            let cur = this.readBinding(pb);
            let isundef = this.b.emit("strict_eq", [cur, this.b.constUndefined()], {});
            let ubool = this.b.emit("to_boolean", [isundef], {});
            let dflt_bb = this.b.newBlock(`default_${pb.name}`);
            let join_bb = this.b.newBlock(`default_join_${pb.name}`);
            this.b.condBr(ubool, dflt_bb, [], join_bb, []);
            this.b.sealBlock(dflt_bb);
            this.b.setInsertPoint(dflt_bb);
            let dv = this.expr(defaults[i]);
            this.writeBinding(pb, dv);
            this.b.br(join_bb, []);
            this.b.sealBlock(join_bb);
            this.b.setInsertPoint(join_bb);
        }

        // hoist function declarations: their closures exist from entry
        for (let binding of info.bindings) {
            if (binding.kind === "fn") {
                let childInfo = this.findChildFn(binding);
                let closure = this.b.emit("make_closure", [this.curEnv], {
                    fn: childInfo.name,
                    name: displayNameOf(childInfo),
                });
                this.writeBinding(binding, closure);
            }
        }
    }

    findChildFn(binding) {
        for (let c of this.info.children) {
            if (c.node.id && c.node.id.name === binding.name) return c;
        }
        throw new Error(`EIR lowering: no child function for binding ${binding.name}`);
    }

    // --- binding access -----------------------------------------------------------

    // Environments form a chain of descriptors: per-iteration loop envs
    // (LoopEnv, parent in slot 0) inside their function's env (FnInfo,
    // parent in parentSlot), which chains to the descriptor current at
    // the function's definition site.  envForBinding walks that chain
    // from the current lowering position to the descriptor holding the
    // binding, emitting one env_load per hop.

    levar(le) {
        return `%loopenv#${le.id}`;
    }

    // the env value make_closure should capture at the current position
    curEnvValue() {
        if (this.activeLoopEnvs.length > 0) {
            let le = this.activeLoopEnvs[this.activeLoopEnvs.length - 1];
            return this.b.readVariable(this.levar(le), this.b.cur);
        }
        return this.curEnv;
    }

    // the innermost materialized descriptor at f's definition site
    descAtCreation(f) {
        let le = f.creationLoopEnv;
        while (le && !le.materialized) le = le.parentCandidate;
        if (le) return le;
        let p = f.parent;
        if (!p) return null;
        if (p.envSize > 0) return p;
        return this.descAtCreation(p);
    }

    // the descriptor whose env value lives in desc's parent slot
    parentDescOf(desc) {
        if (desc.isLoopEnv) {
            // slot 0 holds curEnv at loop entry: the nearest enclosing
            // materialized loop env, else the function env, else the
            // function's creation-site descriptor (== its incoming env)
            let le = desc.parentCandidate;
            while (le && !le.materialized) le = le.parentCandidate;
            if (le) return le;
            if (desc.fnInfo.envSize > 0) return desc.fnInfo;
            return this.descAtCreation(desc.fnInfo);
        }
        // a function env's parent slot holds its incoming env
        return this.descAtCreation(desc);
    }

    // fresh per-iteration env for captured let/const declared in the loop
    // BODY: emitted at the top of the body block each iteration.  their
    // declarations re-execute per pass, so nothing copies forward.
    enterLoopBody(n) {
        let ble = this.analysis.loopBodyEnvOf(n);
        if (!ble) return null;
        let outer = this.curEnvValue();
        let e = this.b.emit("make_env", [], { size: ble.envSize });
        this.b.emit("env_store", [e, outer], { slot: 0 });
        this.b.writeVariable(this.levar(ble), this.b.cur, e);
        this.activeLoopEnvs.push(ble);
        return ble;
    }

    leaveLoopBody(ble) {
        if (ble) this.activeLoopEnvs.pop();
    }

    // a loop lowering claims any labels the enclosing LabeledStatement(s)
    // queued, binding them to its own break/continue blocks
    claimPendingLabels(breakBlock, continueBlock) {
        let n = this.pendingLabels.length;
        for (let name of this.pendingLabels)
            this.activeLabels.push({
                name: name,
                breakBlock: breakBlock,
                continueBlock: continueBlock,
                ctxLen: this.finallyCtx.length,
            });
        this.pendingLabels = [];
        return n;
    }

    releaseLabels(n) {
        while (n-- > 0) this.activeLabels.pop();
    }

    findLabel(name, loc) {
        for (let i = this.activeLabels.length - 1; i >= 0; i--)
            if (this.activeLabels[i].name === name) return this.activeLabels[i];
        throw LowerNotSupported(`unknown label '${name}'`, loc);
    }

    // the environment holding `binding`, from the current position
    envForBinding(binding) {
        let target =
            binding.loopEnv && binding.loopEnv.materialized ? binding.loopEnv : binding.fnInfo;

        let desc, env;
        if (this.activeLoopEnvs.length > 0) {
            desc = this.activeLoopEnvs[this.activeLoopEnvs.length - 1];
            env = this.b.readVariable(this.levar(desc), this.b.cur);
        } else if (this.info.envSize > 0) {
            desc = this.info;
            env = this.curEnv;
        } else {
            desc = this.descAtCreation(this.info);
            env = this.envParam;
        }

        while (desc && desc !== target) {
            let slot = desc.isLoopEnv ? 0 : desc.parentSlot;
            if (slot < 0)
                throw new Error(`EIR lowering: broken env chain through ${desc.name}`);
            env = this.b.emit("env_load", [env], { slot: slot });
            desc = this.parentDescOf(desc);
        }
        if (!desc) throw new Error(`EIR lowering: env chain missed ${binding.uid}`);
        return env;
    }

    readBinding(binding) {
        if (!binding.captured) return this.b.readVariable(binding.uid, this.b.cur);
        let env = this.envForBinding(binding);
        return this.b.emit("env_load", [env], { slot: binding.slot });
    }

    writeBinding(binding, value) {
        if (!binding.captured) {
            this.b.writeVariable(binding.uid, this.b.cur, value);
            return;
        }
        let env = this.envForBinding(binding);
        this.b.emit("env_store", [env, value], { slot: binding.slot });
    }

    // --- expressions ----------------------------------------------------------

    expr(n) {
        switch (n.type) {
            case b.Literal:
                return this.literal(n);
            case b.Identifier:
                return this.identifier(n);
            case b.ThisExpression: {
                // resolved to a binding = an arrow's lexical this (the
                // owner's captured this, read through the env chain)
                let binding = this.analysis.resolve(n);
                if (binding) return this.readBinding(binding);
                return this.b.readVariable("%this", this.b.cur);
            }
            case b.BinaryExpression:
                return this.binary(n);
            case b.LogicalExpression:
                return this.logical(n);
            case b.UnaryExpression:
                return this.unary(n);
            case b.AssignmentExpression:
                return this.assignment(n);
            case b.UpdateExpression:
                return this.update(n);
            case b.TemplateLiteral:
                return this.template(n);
            case b.CallExpression:
                return this.call(n);
            case b.NewExpression:
                return this.newExpr(n);
            case b.MemberExpression:
                return this.member(n);
            case b.ConditionalExpression:
                return this.conditional(n);
            case b.FunctionExpression:
            case b.ArrowFunctionExpression:
                return this.functionExpr(n);
            case b.SequenceExpression: {
                let v;
                for (let e of n.expressions) v = this.expr(e);
                return v;
            }
            case b.ArrayExpression: {
                // holes must stay holes (forEach etc. skip them; undefined
                // wouldn't be skipped).  written with plain loops: the
                // arrow-based form of this case miscompiled under the
                // legacy pipeline (undistilled; see the phase-3 notes).
                let holes = false;
                for (let el of n.elements) if (!el) holes = true;
                if (!holes) {
                    let elems = [];
                    for (let el of n.elements) elems.push(this.expr(el));
                    return this.b.emit("make_array", elems, {});
                }
                let vals = [];
                let indices = [];
                for (let i = 0; i < n.elements.length; i++) {
                    let el = n.elements[i];
                    if (!el) continue;
                    vals.push(this.expr(el));
                    indices.push(i);
                }
                return this.b.emit("make_array", vals, {
                    len: n.elements.length,
                    indices: indices,
                });
            }
            case b.ObjectExpression: {
                let hasAccessors = n.properties.some((p) => p.kind && p.kind !== "init");
                if (hasAccessors) return this.objectWithAccessors(n);
                let hasComputed = n.properties.some(
                    (p) => p.computed || (p.key.type !== b.Identifier && p.key.type !== b.Literal)
                );
                if (!hasComputed) {
                    let keys = [];
                    let values = [];
                    for (let p of n.properties) {
                        keys.push(p.key.type === b.Identifier ? p.key.name : String(p.key.value));
                        values.push(this.expr(p.value));
                    }
                    return this.b.emit("make_object", values, { keys: keys });
                }
                // computed keys: empty object + per-property stores in
                // source order (key evaluates before value, per spec)
                let obj = this.b.emit("make_object", [], { keys: [] });
                for (let p of n.properties) {
                    if (!p.computed && (p.key.type === b.Identifier || p.key.type === b.Literal)) {
                        let v = this.expr(p.value);
                        this.b.emit("set_prop_atom", [obj, v], {
                            atom: p.key.type === b.Identifier ? p.key.name : String(p.key.value),
                        });
                    } else {
                        let k = this.expr(p.key);
                        let v = this.expr(p.value);
                        this.b.emit("set_prop", [obj, k, v], {});
                    }
                }
                return obj;
            }
            default:
                throw LowerNotSupported(`expression type ${n.type}`, n.loc);
        }
    }

    // an object literal containing get/set accessors: empty object, then
    // per-property defines in source order.  a get/set PAIR for one name
    // becomes a single define_accessor (name-keyed — keying by the key
    // AST node is how the class desugar lost getters, bug #14).
    objectWithAccessors(n) {
        let obj = this.b.emit("make_object", [], { keys: [] });
        let done = new Set();
        for (let i = 0; i < n.properties.length; i++) {
            let p = n.properties[i];
            if (p.computed || (p.key.type !== b.Identifier && p.key.type !== b.Literal))
                throw LowerNotSupported("computed key in accessor object literal", n.loc);
            let name = p.key.type === b.Identifier ? p.key.name : String(p.key.value);
            if (p.kind && p.kind !== "init") {
                if (done.has(name)) continue; // the pair lowered together
                done.add(name);
                let getter = null;
                let setter = null;
                for (let j = i; j < n.properties.length; j++) {
                    let q = n.properties[j];
                    if (q.kind === "init" || q.computed) continue;
                    let qname = q.key.type === b.Identifier ? q.key.name : String(q.key.value);
                    if (qname !== name) continue;
                    if (q.kind === "get") getter = this.expr(q.value);
                    else if (q.kind === "set") setter = this.expr(q.value);
                }
                this.b.emit(
                    "define_accessor",
                    [obj, getter || this.b.constUndefined(), setter || this.b.constUndefined()],
                    { atom: name }
                );
            } else {
                let v = this.expr(p.value);
                this.b.emit("set_prop_atom", [obj, v], { atom: name });
            }
        }
        return obj;
    }

    literal(n) {
        if (n.value === null) return this.b.constNull();
        switch (typeof n.value) {
            case "number":
                return this.b.constNumber(n.value);
            case "string":
                return this.b.constAtom(n.value);
            case "boolean":
                return this.b.constBool(n.value);
            case "object": {
                // a regex literal: fresh RegExp per evaluation, like the
                // legacy visitLiteral
                if (typeof n.value.source !== "string")
                    throw LowerNotSupported(`literal ${typeof n.value}`, n.loc);
                let flags =
                    (n.value.global ? "g" : "") +
                    (n.value.multiline ? "m" : "") +
                    (n.value.ignoreCase ? "i" : "");
                return this.b.emit("make_regexp", [], {
                    source: n.value.source,
                    flags: flags,
                });
            }
            default:
                throw LowerNotSupported(`literal ${typeof n.value}`, n.loc);
        }
    }

    identifier(n) {
        if (n.name === "undefined") return this.b.constUndefined();
        let binding = this.analysis.resolve(n);
        if (binding === null || binding === undefined) {
            let ref = this.mod_ctx.refs.get(n.name);
            if (ref) {
                if (ref.exotic !== undefined)
                    return this.b.emit("module_get_exotic", [], { module: ref.exotic });
                if (ref.constval !== undefined) return this.literal(ref.constval);
                return this.b.emit("module_slot_load", [], {
                    module: ref.module,
                    slot: ref.slot,
                });
            }
            if (this.mod_ctx.siblings.has(n.name))
                throw LowerNotSupported(
                    `module function '${n.name}' used as a value`,
                    n.loc
                );
            return this.b.emit("get_global", [], { atom: n.name });
        }
        if (binding.kind === "self")
            throw LowerNotSupported("function self-reference as a value", n.loc);
        return this.readBinding(binding);
    }

    functionExpr(n) {
        let childInfo = this.analysis.infoFor(n);
        if (!childInfo) throw new Error("EIR lowering: unanalyzed function expression");
        lowerOneFunction(childInfo, this.analysis, this.module, this.mod_ctx);
        // capture the innermost env: the current iteration's loop env when
        // inside a for-let loop, else the function env / incoming env
        return this.b.emit("make_closure", [this.curEnvValue()], {
            fn: childInfo.name,
            name: displayNameOf(childInfo),
        });
    }

    binary(n) {
        let op = binops[n.operator];
        if (!op) throw LowerNotSupported(`binary operator ${n.operator}`, n.loc);
        let l = this.expr(n.left);
        let r = this.expr(n.right);
        return this.b.emit(op, [l, r], {});
    }

    logical(n) {
        let l = this.expr(n.left);
        let lbool = this.b.emit("to_boolean", [l], {});

        let rhs_bb = this.b.newBlock("logical_rhs");
        let join_bb = this.b.newBlock("logical_join");
        let result = join_bb.addParam("logical");

        if (n.operator === "&&") this.b.condBr(lbool, rhs_bb, [], join_bb, [l]);
        else if (n.operator === "||") this.b.condBr(lbool, join_bb, [l], rhs_bb, []);
        else throw LowerNotSupported(`logical operator ${n.operator}`, n.loc);
        this.b.sealBlock(rhs_bb);

        this.b.setInsertPoint(rhs_bb);
        let r = this.expr(n.right);
        this.b.br(join_bb, [r]);
        this.b.sealBlock(join_bb);

        this.b.setInsertPoint(join_bb);
        return result;
    }

    unary(n) {
        let arg;
        switch (n.operator) {
            case "!":
                arg = this.expr(n.argument);
                return this.b.emit("logical_not", [arg], {});
            case "-":
                arg = this.expr(n.argument);
                return this.b.emit("neg", [arg], {});
            case "+":
                arg = this.expr(n.argument);
                return this.b.emit("unary_plus", [arg], {});
            case "~":
                arg = this.expr(n.argument);
                return this.b.emit("bitnot", [arg], {});
            case "typeof":
                arg = this.expr(n.argument);
                return this.b.emit("typeof", [arg], {});
            case "void":
                // evaluate for side effects, produce undefined (the
                // desugar passes' undefinedLit() emits `void 0`)
                this.expr(n.argument);
                return this.b.constUndefined();
            case "delete": {
                // only member expressions (matching the legacy visitUnary)
                let m = n.argument;
                let obj = this.expr(m.object);
                let key;
                if (!m.computed && m.property.type === b.Identifier)
                    key = this.b.constAtom(m.property.name);
                else key = this.expr(m.property);
                return this.b.emit("delete_prop", [obj, key], {});
            }
            default:
                throw LowerNotSupported(`unary operator ${n.operator}`, n.loc);
        }
    }

    // the one-time declaration store for a slot-backed toplevel binding:
    // unlike writeIdentifier this may store to read-only refs (an exported
    // const's initializer is a legitimate store)
    writeModuleSlotInit(idNode, value) {
        let ref = this.mod_ctx.refs.get(idNode.name);
        if (!ref || ref.module === undefined || ref.slot === undefined || ref.slot < 0)
            throw LowerNotSupported(
                `toplevel declaration of '${idNode.name}' has no slot`,
                idNode.loc
            );
        this.b.emit("module_slot_store", [value], { module: ref.module, slot: ref.slot });
    }

    // store `value` into this module's export slot named `exportName`
    storeExportSlot(exportName, value, loc) {
        let tmi = this.mod_ctx.this_module_info;
        let export_info = tmi && tmi.exports.get(exportName);
        if (!export_info)
            throw LowerNotSupported(`no export slot for '${exportName}'`, loc);
        this.b.emit("module_slot_store", [value], {
            module: "%self",
            slot: export_info.slot_num,
        });
    }

    // store `value` into the identifier `idNode` (local binding, writable
    // module slot, or global)
    writeIdentifier(idNode, value) {
        let binding = this.analysis.resolve(idNode);
        if (binding === null || binding === undefined) {
            let ref = this.mod_ctx.refs.get(idNode.name);
            if (ref) {
                if (!ref.writable)
                    throw LowerNotSupported(
                        `assignment to read-only module binding '${idNode.name}'`,
                        idNode.loc
                    );
                this.b.emit("module_slot_store", [value], {
                    module: ref.module,
                    slot: ref.slot,
                });
                return;
            }
            if (this.mod_ctx.siblings.has(idNode.name))
                throw LowerNotSupported(
                    `assignment to module function '${idNode.name}'`,
                    idNode.loc
                );
            this.b.emit("set_global", [value], { atom: idNode.name });
            return;
        }
        this.writeBinding(binding, value);
    }

    assignment(n) {
        let binop = n.operator === "=" ? null : binops[compound_assign_ops[n.operator]];
        if (n.operator !== "=" && !binop)
            throw LowerNotSupported(`assignment operator ${n.operator}`, n.loc);
        if (n.left.type === b.Identifier) {
            let v;
            if (binop) {
                let cur = this.identifier(n.left);
                let rhs = this.expr(n.right);
                v = this.b.emit(binop, [cur, rhs], {});
            } else {
                v = this.expr(n.right);
            }
            this.writeIdentifier(n.left, v);
            return v;
        }
        if (n.left.type === b.MemberExpression) {
            // evaluate the object (and computed key) exactly once
            let obj = this.expr(n.left.object);
            let atom = null;
            let key = null;
            if (!n.left.computed && n.left.property.type === b.Identifier)
                atom = n.left.property.name;
            else key = this.expr(n.left.property);
            let v;
            if (binop) {
                let cur =
                    atom !== null
                        ? this.b.emit("get_prop_atom", [obj], { atom: atom })
                        : this.b.emit("get_prop", [obj, key], {});
                let rhs = this.expr(n.right);
                v = this.b.emit(binop, [cur, rhs], {});
            } else {
                v = this.expr(n.right);
            }
            if (atom !== null) this.b.emit("set_prop_atom", [obj, v], { atom: atom });
            else this.b.emit("set_prop", [obj, key, v], {});
            return v;
        }
        throw LowerNotSupported(`assignment target ${n.left.type}`, n.loc);
    }

    // ++/--: ToNumber(old value) via unary_plus, then add/sub 1
    update(n) {
        let one = this.b.constNumber(1);
        let op = n.operator === "++" ? "add" : "sub";
        if (n.argument.type === b.Identifier) {
            let cur = this.identifier(n.argument);
            let old = this.b.emit("unary_plus", [cur], {});
            let nv = this.b.emit(op, [old, one], {});
            this.writeIdentifier(n.argument, nv);
            return n.prefix ? nv : old;
        }
        if (n.argument.type === b.MemberExpression) {
            let m = n.argument;
            let obj = this.expr(m.object);
            let atom = null;
            let key = null;
            if (!m.computed && m.property.type === b.Identifier) atom = m.property.name;
            else key = this.expr(m.property);
            let cur =
                atom !== null
                    ? this.b.emit("get_prop_atom", [obj], { atom: atom })
                    : this.b.emit("get_prop", [obj, key], {});
            let old = this.b.emit("unary_plus", [cur], {});
            let nv = this.b.emit(op, [old, one], {});
            if (atom !== null) this.b.emit("set_prop_atom", [obj, nv], { atom: atom });
            else this.b.emit("set_prop", [obj, key, nv], {});
            return n.prefix ? nv : old;
        }
        throw LowerNotSupported(`update of ${n.argument.type}`, n.loc);
    }

    // untagged template literal: the inlined default handler — zip cooked
    // strings and ToString'ed substitutions with string_concat (matching
    // the legacy handleTemplateDefaultHandlerCall)
    template(n) {
        let strval = null;
        let concat = (s) => {
            if (!strval) strval = s;
            else strval = this.b.emit("call_runtime", [strval, s], { name: "string_concat" });
        };
        for (let i = 0; i < n.quasis.length; i++) {
            let cooked = n.quasis[i].value.cooked;
            if (cooked.length !== 0) concat(this.b.constAtom(cooked));
            if (i < n.expressions.length) {
                let sub = this.expr(n.expressions[i]);
                concat(this.b.emit("call_runtime", [sub], { name: "ToString" }));
            }
        }
        return strval || this.b.constAtom("");
    }

    // `ns.member` where ns is a namespace import of a JS module resolves
    // to a slot load at compile time (mirroring new-cc's rewrite): the
    // module object doesn't answer runtime property lookups for its
    // exports.  native ("@...") modules DO — they keep the runtime path.
    // returns the loaded value, or null if this isn't such an access.
    exoticMemberLoad(n) {
        if (n.object.type !== b.Identifier) return null;
        let binding = this.analysis.resolve(n.object);
        if (binding !== null && binding !== undefined) return null; // shadowed
        let ref = this.mod_ctx.refs.get(n.object.name);
        if (!ref || ref.exotic === undefined || !ref.module_info) return null;
        if (ref.exotic[0] === "@") return null; // native: runtime lookup works
        let name = null;
        if (!n.computed && n.property.type === b.Identifier) name = n.property.name;
        else if (n.property.type === b.Literal && typeof n.property.value === "string")
            name = n.property.value;
        if (name === null) return null;
        let export_info = ref.module_info.exports.get(name);
        if (!export_info || export_info.promoted) return null; // promoted slots are private
        let cv = export_info.constval;
        if (cv && cv.type === b.Literal && (cv.value === null || typeof cv.value !== "object"))
            return this.literal(cv);
        return this.b.emit("module_slot_load", [], {
            module: ref.exotic,
            slot: export_info.slot_num,
        });
    }

    member(n) {
        let slotv = this.exoticMemberLoad(n);
        if (slotv) return slotv;
        let obj = this.expr(n.object);
        if (!n.computed && n.property.type === b.Identifier)
            return this.b.emit("get_prop_atom", [obj], { atom: n.property.name });
        let key = this.expr(n.property);
        return this.b.emit("get_prop", [obj, key], {});
    }

    call(n) {
        // %-intrinsic calls from the pre-EIR desugar passes lower through
        // the table in intrinsics.js (scopes.js already rejected unknowns)
        if (n.callee.type === b.Identifier && n.callee.name[0] === "%")
            return this.intrinsicCall(n);
        let callee, thisArg;
        if (n.callee.type === b.MemberExpression) {
            // ns.member(...) on a JS namespace import: the callee resolves
            // to a slot load and `this` is undefined (the legacy rewrite
            // turns the member expression into %moduleGetSlot before call
            // handling ever sees it)
            let slotCallee = this.exoticMemberLoad(n.callee);
            if (slotCallee) {
                let args = n.arguments.map((a) => this.expr(a));
                return this.b.emit(
                    "call",
                    [slotCallee, this.b.constUndefined()].concat(args),
                    {}
                );
            }
            thisArg = this.expr(n.callee.object);
            if (!n.callee.computed && n.callee.property.type === b.Identifier)
                callee = this.b.emit("get_prop_atom", [thisArg], {
                    atom: n.callee.property.name,
                });
            else {
                let key = this.expr(n.callee.property);
                callee = this.b.emit("get_prop", [thisArg, key], {});
            }
        } else {
            // direct calls: recursion through the self binding, and calls
            // to sibling top-level EIR functions, skip closure dispatch
            if (n.callee.type === b.Identifier) {
                let binding = this.analysis.resolve(n.callee);
                if (binding && binding.kind === "self" && binding.fnInfo === this.info) {
                    let dthis = this.b.constUndefined();
                    let dargs = n.arguments.map((a) => this.expr(a));
                    return this.b.emit("call", [this.envParam, dthis].concat(dargs), {
                        direct: this.info.name,
                    });
                }
                if (
                    (binding === null || binding === undefined) &&
                    this.mod_ctx.siblings.has(n.callee.name)
                ) {
                    let dthis = this.b.constUndefined();
                    let dargs = n.arguments.map((a) => this.expr(a));
                    return this.b.emit("call", [this.envParam, dthis].concat(dargs), {
                        direct: this.mod_ctx.siblings.get(n.callee.name),
                    });
                }
            }
            callee = this.expr(n.callee);
            thisArg = this.b.constUndefined();
        }
        let args = n.arguments.map((a) => this.expr(a));
        return this.b.emit("call", [callee, thisArg].concat(args), {});
    }

    intrinsicCall(n) {
        let intr = eir_intrinsics[n.callee.name];
        if (!intr) throw LowerNotSupported(`intrinsic ${n.callee.name}`, n.loc);
        let args = n.arguments.map((a) => this.expr(a));
        let v;
        if (intr.op) v = this.b.emit(intr.op, args, {});
        else v = this.b.emit("call_runtime", args, { name: intr.runtime, void: intr.void });
        // super() in a derived constructor: the constructed object becomes
        // `this` for the rest of the function — including the env copy
        // arrows read their lexical this from
        if (intr.rebindThis) {
            this.b.writeVariable("%this", this.b.cur, v);
            if (this.info.thisBinding && this.info.thisBinding.captured)
                this.writeBinding(this.info.thisBinding, v);
        }
        return v;
    }

    newExpr(n) {
        let callee = this.expr(n.callee);
        let args = n.arguments.map((a) => this.expr(a));
        return this.b.emit("construct", [callee].concat(args), {});
    }

    conditional(n) {
        let cond = this.expr(n.test);
        let cbool = this.b.emit("to_boolean", [cond], {});

        let then_bb = this.b.newBlock("cond_then");
        let else_bb = this.b.newBlock("cond_else");
        let join_bb = this.b.newBlock("cond_join");
        let result = join_bb.addParam("cond");

        this.b.condBr(cbool, then_bb, [], else_bb, []);
        this.b.sealBlock(then_bb);
        this.b.sealBlock(else_bb);

        this.b.setInsertPoint(then_bb);
        let tv = this.expr(n.consequent);
        this.b.br(join_bb, [tv]);

        this.b.setInsertPoint(else_bb);
        let ev = this.expr(n.alternate);
        this.b.br(join_bb, [ev]);
        this.b.sealBlock(join_bb);

        this.b.setInsertPoint(join_bb);
        return result;
    }

    // --- statements ---------------------------------------------------------------

    stmt(n) {
        switch (n.type) {
            case b.BlockStatement:
                for (let s of n.body) {
                    this.stmt(s);
                    if (this.b.cur.terminated) return;
                }
                return;
            case b.VariableDeclaration:
                for (let d of n.declarations) {
                    if (d.id.type === b.ObjectPattern) {
                        this.lowerObjectPatternDecl(d);
                        continue;
                    }
                    if (d.id.type !== b.Identifier)
                        throw LowerNotSupported(`declaration pattern ${d.id.type}`, n.loc);
                    let binding = this.analysis.resolve(d.id);
                    if (!binding && this.isToplevel) {
                        // a slot-backed module declarator (analyzeToplevel
                        // declared no local): store the initializer through
                        // the slot.  const-literal folds have no storage —
                        // their (literal) initializer is dropped.
                        let ref = this.mod_ctx.refs.get(d.id.name);
                        if (ref && ref.module === null) continue;
                        let v = d.init ? this.expr(d.init) : this.b.constUndefined();
                        this.writeModuleSlotInit(d.id, v);
                        continue;
                    }
                    // visible-as-undefined during its own initializer: a
                    // direct self-reference reads undefined, and a closure
                    // in the init captures the (env) binding the real value
                    // is stored into below.  free for uncaptured bindings
                    // (SSA map write only).
                    this.writeBinding(binding, this.b.constUndefined());
                    let init = d.init ? this.expr(d.init) : this.b.constUndefined();
                    this.writeBinding(binding, init);
                }
                return;
            case b.FunctionDeclaration: {
                let binding = this.analysis.resolve(n.id);
                if (!binding && this.isToplevel) {
                    // a slot-backed module function: lower it, then store
                    // its closure to the slot at this statement's source
                    // position (same hoisting caveat as the legacy
                    // %moduleSetSlot rewrite)
                    let childInfo = this.analysis.infoFor(n);
                    lowerOneFunction(childInfo, this.analysis, this.module, this.mod_ctx);
                    let closure = this.b.emit("make_closure", [this.curEnvValue()], {
                        fn: childInfo.name,
                        name: displayNameOf(childInfo),
                    });
                    this.writeModuleSlotInit(n.id, closure);
                    return;
                }
                // closure was created (hoisted) at entry; lower the body now
                lowerOneFunction(this.analysis.infoFor(n), this.analysis, this.module, this.mod_ctx);
                return;
            }
            case b.ImportDeclaration:
                if (!this.isToplevel) throw LowerNotSupported("import declaration", n.loc);
                // module resolution happens in the toplevel scaffolding;
                // a bare `import "m"` also touches the module object for
                // parity with the legacy %moduleGetExotic rewrite
                if (n.specifiers.length === 0)
                    this.b.emit("module_get_exotic", [], { module: n.source_path.value });
                return;
            case b.ExportNamedDeclaration: {
                if (!this.isToplevel) throw LowerNotSupported("export declaration", n.loc);
                if (n.declaration && !Array.isArray(n.declaration)) return this.stmt(n.declaration);
                // export { A, B as C }: copy the locals' current values
                // into the exported slots at this statement's position
                for (let spec of n.specifiers) {
                    let v = this.identifier(spec.local);
                    this.storeExportSlot(spec.exported.name, v, n.loc);
                }
                return;
            }
            case b.ExportDefaultDeclaration: {
                if (!this.isToplevel) throw LowerNotSupported("export default", n.loc);
                let v = this.expr(n.declaration);
                this.storeExportSlot("default", v, n.loc);
                return;
            }
            case b.ExpressionStatement:
                this.expr(n.expression);
                return;
            case b.IfStatement:
                return this.ifStmt(n);
            case b.WhileStatement:
                return this.whileStmt(n);
            case b.DoWhileStatement:
                return this.doWhileStmt(n);
            case b.ForStatement:
                return this.forStmt(n);
            case b.ForOfStatement:
                return this.forOfStmt(n);
            case b.ForInStatement:
                return this.forInStmt(n);
            case b.SwitchStatement:
                return this.switchStmt(n);
            case b.ReturnStatement: {
                let rv = n.argument ? this.expr(n.argument) : this.b.constUndefined();
                if (this.finallyCtx.length > 0) {
                    if (this.runFinalizers(0)) return; // a finalizer overrode control
                }
                this.b.ret(rv);
                return;
            }
            case b.ThrowStatement:
                this.b.throwValue(this.expr(n.argument));
                return;
            case b.TryStatement:
                return this.tryStmt(n);
            case b.LabeledStatement: {
                // labels on loops bind to the loop's own blocks (the loop
                // lowering claims them); labels on anything else get a
                // synthetic exit block for labeled breaks
                let body = n.body;
                while (body.type === b.LabeledStatement) body = body.body;
                let isLoop =
                    body.type === b.WhileStatement ||
                    body.type === b.DoWhileStatement ||
                    body.type === b.ForStatement ||
                    body.type === b.ForInStatement ||
                    body.type === b.ForOfStatement;
                if (isLoop) {
                    this.pendingLabels.push(n.label.name);
                    this.stmt(n.body);
                    return;
                }
                let exit = this.b.newBlock(`label_${n.label.name}`);
                this.activeLabels.push({
                    name: n.label.name,
                    breakBlock: exit,
                    continueBlock: null,
                    ctxLen: this.finallyCtx.length,
                });
                this.stmt(n.body);
                this.activeLabels.pop();
                if (!this.b.cur.terminated) this.b.br(exit, []);
                this.b.sealBlock(exit);
                this.b.setInsertPoint(exit);
                return;
            }
            case b.BreakStatement: {
                if (n.label) {
                    let l = this.findLabel(n.label.name, n.loc);
                    if (this.finallyCtx.length > l.ctxLen) {
                        if (this.runFinalizers(l.ctxLen)) return;
                    }
                    this.b.br(l.breakBlock, []);
                    return;
                }
                if (this.breakTargets.length === 0)
                    throw LowerNotSupported("break outside plain loop/switch", n.loc);
                let targetLen = this.breakTargets.length;
                let firstCrossed = this.finallyCtx.findIndex((c) => c.breakDepth >= targetLen);
                if (firstCrossed !== -1) {
                    if (this.runFinalizers(firstCrossed)) return;
                }
                this.b.br(this.breakTargets[targetLen - 1], []);
                return;
            }
            case b.ContinueStatement: {
                if (n.label) {
                    let l = this.findLabel(n.label.name, n.loc);
                    if (!l.continueBlock)
                        throw LowerNotSupported(`continue to non-loop label '${n.label.name}'`, n.loc);
                    if (this.finallyCtx.length > l.ctxLen) {
                        if (this.runFinalizers(l.ctxLen)) return;
                    }
                    this.b.br(l.continueBlock, []);
                    return;
                }
                if (this.continueTargets.length === 0)
                    throw LowerNotSupported("continue outside plain loop", n.loc);
                let targetLen = this.continueTargets.length;
                let firstCrossed = this.finallyCtx.findIndex((c) => c.continueDepth >= targetLen);
                if (firstCrossed !== -1) {
                    if (this.runFinalizers(firstCrossed)) return;
                }
                this.b.br(this.continueTargets[targetLen - 1], []);
                return;
            }
            case b.EmptyStatement:
                return;
            default:
                throw LowerNotSupported(`statement type ${n.type}`, n.loc);
        }
    }

    ifStmt(n) {
        let cond = this.expr(n.test);
        let cbool = this.b.emit("to_boolean", [cond], {});

        let then_bb = this.b.newBlock("if_then");
        let else_bb = n.alternate ? this.b.newBlock("if_else") : null;
        let join_bb = this.b.newBlock("if_join");

        this.b.condBr(cbool, then_bb, [], else_bb || join_bb, []);
        this.b.sealBlock(then_bb);
        if (else_bb) this.b.sealBlock(else_bb);

        this.b.setInsertPoint(then_bb);
        this.stmt(n.consequent);
        if (!this.b.cur.terminated) this.b.br(join_bb, []);

        if (else_bb) {
            this.b.setInsertPoint(else_bb);
            this.stmt(n.alternate);
            if (!this.b.cur.terminated) this.b.br(join_bb, []);
        }
        this.b.sealBlock(join_bb);
        this.b.setInsertPoint(join_bb);
    }

    whileStmt(n) {
        let header = this.b.newBlock("while_header");
        let body = this.b.newBlock("while_body");
        let exit = this.b.newBlock("while_exit");

        this.b.br(header, []);

        this.b.setInsertPoint(header);
        let cond = this.expr(n.test);
        let cbool = this.b.emit("to_boolean", [cond], {});
        this.b.condBr(cbool, body, [], exit, []);
        this.b.sealBlock(body);

        this.breakTargets.push(exit);
        this.continueTargets.push(header);
        let nlabels = this.claimPendingLabels(exit, header);
        this.b.setInsertPoint(body);
        let ble = this.enterLoopBody(n);
        this.stmt(n.body);
        this.leaveLoopBody(ble);
        if (!this.b.cur.terminated) this.b.br(header, []);
        this.releaseLabels(nlabels);
        this.breakTargets.pop();
        this.continueTargets.pop();

        this.b.sealBlock(header);
        this.b.sealBlock(exit);
        this.b.setInsertPoint(exit);
    }

    doWhileStmt(n) {
        let body = this.b.newBlock("do_body");
        let cond_bb = this.b.newBlock("do_cond");
        let exit = this.b.newBlock("do_exit");

        this.b.br(body, []);

        this.breakTargets.push(exit);
        this.continueTargets.push(cond_bb);
        let nlabels = this.claimPendingLabels(exit, cond_bb);
        this.b.setInsertPoint(body);
        let ble = this.enterLoopBody(n);
        this.stmt(n.body);
        this.leaveLoopBody(ble);
        if (!this.b.cur.terminated) this.b.br(cond_bb, []);
        this.releaseLabels(nlabels);
        this.breakTargets.pop();
        this.continueTargets.pop();
        this.b.sealBlock(cond_bb);

        this.b.setInsertPoint(cond_bb);
        let cond = this.expr(n.test);
        let cbool = this.b.emit("to_boolean", [cond], {});
        this.b.condBr(cbool, body, [], exit, []);
        this.b.sealBlock(body);
        this.b.sealBlock(exit);
        this.b.setInsertPoint(exit);
    }

    forStmt(n) {
        // captured let/const loop vars live in a fresh env per iteration:
        // the initial env is created before the init declaration runs, and
        // each pass through the update block makes a new env, copying the
        // loop vars forward (so the update and next test see the copies,
        // and closures made in earlier iterations keep their own)
        let le = this.analysis.loopEnvOf(n);
        let outerEnvVal = null;
        if (le) {
            outerEnvVal = this.curEnvValue();
            let e = this.b.emit("make_env", [], { size: le.envSize });
            this.b.emit("env_store", [e, outerEnvVal], { slot: 0 });
            this.b.writeVariable(this.levar(le), this.b.cur, e);
            this.activeLoopEnvs.push(le);
        }

        if (n.init) {
            if (n.init.type === b.VariableDeclaration) this.stmt(n.init);
            else this.expr(n.init);
        }

        let header = this.b.newBlock("for_header");
        let body = this.b.newBlock("for_body");
        let update = this.b.newBlock("for_update");
        let exit = this.b.newBlock("for_exit");

        this.b.br(header, []);

        this.b.setInsertPoint(header);
        if (n.test) {
            let cond = this.expr(n.test);
            let cbool = this.b.emit("to_boolean", [cond], {});
            this.b.condBr(cbool, body, [], exit, []);
        } else {
            this.b.br(body, []);
        }
        this.b.sealBlock(body);

        this.breakTargets.push(exit);
        this.continueTargets.push(update);
        let nlabels = this.claimPendingLabels(exit, update);
        this.b.setInsertPoint(body);
        let ble = this.enterLoopBody(n);
        this.stmt(n.body);
        this.leaveLoopBody(ble);
        if (!this.b.cur.terminated) this.b.br(update, []);
        this.releaseLabels(nlabels);
        this.breakTargets.pop();
        this.continueTargets.pop();
        this.b.sealBlock(update);

        this.b.setInsertPoint(update);
        if (le) {
            let eold = this.b.readVariable(this.levar(le), this.b.cur);
            let enew = this.b.emit("make_env", [], { size: le.envSize });
            this.b.emit("env_store", [enew, outerEnvVal], { slot: 0 });
            for (let bd of le.bindings) {
                let v = this.b.emit("env_load", [eold], { slot: bd.slot });
                this.b.emit("env_store", [enew, v], { slot: bd.slot });
            }
            this.b.writeVariable(this.levar(le), this.b.cur, enew);
        }
        if (n.update) this.expr(n.update);
        this.b.br(header, []);
        this.b.sealBlock(header);
        this.b.sealBlock(exit);
        if (le) this.activeLoopEnvs.pop();
        this.b.setInsertPoint(exit);
    }

    lowerObjectPatternDecl(d) {
        let src = d.init ? this.expr(d.init) : this.b.constUndefined();
        for (let prop of d.id.properties) {
            let keyName =
                prop.key.type === b.Identifier ? prop.key.name : String(prop.key.value);
            let target = prop.value;
            let dflt = null;
            if (target.type === b.AssignmentPattern) {
                dflt = target.right;
                target = target.left;
            }
            let binding = this.analysis.resolve(target);
            let v = this.b.emit("get_prop_atom", [src], { atom: keyName });
            this.writeBinding(binding, v);
            if (dflt) {
                let isundef = this.b.emit("strict_eq", [v, this.b.constUndefined()], {});
                let ubool = this.b.emit("to_boolean", [isundef], {});
                let dflt_bb = this.b.newBlock(`pat_default_${keyName}`);
                let join_bb = this.b.newBlock(`pat_join_${keyName}`);
                this.b.condBr(ubool, dflt_bb, [], join_bb, []);
                this.b.sealBlock(dflt_bb);
                this.b.setInsertPoint(dflt_bb);
                let dv = this.expr(dflt);
                this.writeBinding(binding, dv);
                this.b.br(join_bb, []);
                this.b.sealBlock(join_bb);
                this.b.setInsertPoint(join_bb);
            }
        }
    }

    // mirrors the legacy DesugarForOf expansion: iterable[Symbol.iterator]()
    // once, then `next()` per iteration, testing `.done` and binding `.value`
    forOfStmt(n) {
        // a captured let/const loop var gets a fresh env each iteration
        // (created at the top of the body, right before the var is bound);
        // no copying between iterations — the binding is (re)assigned from
        // the iteration value anyway.  an initial env exists before the
        // RHS evaluates: scope analysis declares the binding before
        // walking the RHS, so a closure there may already capture it
        // (reading undefined, matching the legacy alloca behavior).
        let le = this.analysis.loopEnvOf(n);
        let outerEnvVal = null;
        if (le) {
            outerEnvVal = this.curEnvValue();
            let e0 = this.b.emit("make_env", [], { size: le.envSize });
            this.b.emit("env_store", [e0, outerEnvVal], { slot: 0 });
            this.b.writeVariable(this.levar(le), this.b.cur, e0);
            this.activeLoopEnvs.push(le);
        }

        let obj = this.expr(n.right);
        let sym = this.b.emit("get_global", [], { atom: "Symbol" });
        let itkey = this.b.emit("get_prop_atom", [sym], { atom: "iterator" });
        let itfn = this.b.emit("get_prop", [obj, itkey], {});
        let iter = this.b.emit("call", [itfn, obj], {});

        let header = this.b.newBlock("forof_header");
        let body = this.b.newBlock("forof_body");
        let exit = this.b.newBlock("forof_exit");

        this.b.br(header, []);

        this.b.setInsertPoint(header);
        let nextfn = this.b.emit("get_prop_atom", [iter], { atom: "next" });
        let res = this.b.emit("call", [nextfn, iter], {});
        let done = this.b.emit("get_prop_atom", [res], { atom: "done" });
        let dbool = this.b.emit("to_boolean", [done], {});
        this.b.condBr(dbool, exit, [], body, []);
        this.b.sealBlock(body);

        this.b.setInsertPoint(body);
        if (le) {
            let e = this.b.emit("make_env", [], { size: le.envSize });
            this.b.emit("env_store", [e, outerEnvVal], { slot: 0 });
            this.b.writeVariable(this.levar(le), this.b.cur, e);
        }
        let v = this.b.emit("get_prop_atom", [res], { atom: "value" });
        if (n.left.type === b.VariableDeclaration) {
            let binding = this.analysis.resolve(n.left.declarations[0].id);
            this.writeBinding(binding, v);
        } else {
            this.writeIdentifier(n.left, v);
        }
        let ble = this.enterLoopBody(n);
        this.breakTargets.push(exit);
        this.continueTargets.push(header);
        let nlabels = this.claimPendingLabels(exit, header);
        this.stmt(n.body);
        this.leaveLoopBody(ble);
        if (!this.b.cur.terminated) this.b.br(header, []);
        this.releaseLabels(nlabels);
        this.breakTargets.pop();
        this.continueTargets.pop();

        this.b.sealBlock(header);
        this.b.sealBlock(exit);
        if (le) this.activeLoopEnvs.pop();
        this.b.setInsertPoint(exit);
    }

    // mirrors the legacy visitForIn: prop_iterator_new once, then
    // prop_iterator_next / prop_iterator_current per iteration.  the
    // iterator value is opaque (not an ejsval) and must stay a direct
    // instruction reference — never a block argument.
    forInStmt(n) {
        // fresh env per iteration for a captured let/const binding, with
        // an initial env before the RHS evaluates — as in forOfStmt
        let le = this.analysis.loopEnvOf(n);
        let outerEnvVal = null;
        if (le) {
            outerEnvVal = this.curEnvValue();
            let e0 = this.b.emit("make_env", [], { size: le.envSize });
            this.b.emit("env_store", [e0, outerEnvVal], { slot: 0 });
            this.b.writeVariable(this.levar(le), this.b.cur, e0);
            this.activeLoopEnvs.push(le);
        }

        let obj = this.expr(n.right);
        let iter = this.b.emit("prop_iter_new", [obj], {});

        let header = this.b.newBlock("forin_header");
        let body = this.b.newBlock("forin_body");
        let exit = this.b.newBlock("forin_exit");

        this.b.br(header, []);

        this.b.setInsertPoint(header);
        let more = this.b.emit("prop_iter_next", [iter], {}); // i1
        this.b.condBr(more, body, [], exit, []);
        this.b.sealBlock(body);

        this.b.setInsertPoint(body);
        if (le) {
            let e = this.b.emit("make_env", [], { size: le.envSize });
            this.b.emit("env_store", [e, outerEnvVal], { slot: 0 });
            this.b.writeVariable(this.levar(le), this.b.cur, e);
        }
        let v = this.b.emit("prop_iter_current", [iter], {});
        if (n.left.type === b.VariableDeclaration) {
            let binding = this.analysis.resolve(n.left.declarations[0].id);
            this.writeBinding(binding, v);
        } else {
            this.writeIdentifier(n.left, v);
        }
        let ble = this.enterLoopBody(n);
        this.breakTargets.push(exit);
        this.continueTargets.push(header);
        let nlabels = this.claimPendingLabels(exit, header);
        this.stmt(n.body);
        this.leaveLoopBody(ble);
        if (!this.b.cur.terminated) this.b.br(header, []);
        this.releaseLabels(nlabels);
        this.breakTargets.pop();
        this.continueTargets.pop();

        this.b.sealBlock(header);
        this.b.sealBlock(exit);
        if (le) this.activeLoopEnvs.pop();
        this.b.setInsertPoint(exit);
    }

    switchStmt(n) {
        let disc = this.expr(n.discriminant);
        let exit = this.b.newBlock("switch_exit");
        let bodies = n.cases.map((c, i) => this.b.newBlock(`case_body${i}`));
        let defaultIdx = n.cases.findIndex((c) => !c.test);

        // test chain, in document order, skipping default
        for (let i = 0; i < n.cases.length; i++) {
            if (!n.cases[i].test) continue;
            let tv = this.expr(n.cases[i].test);
            let cmp = this.b.emit("strict_eq", [disc, tv], {});
            let cbool = this.b.emit("to_boolean", [cmp], {});
            let next_test = this.b.newBlock(`case_test${i}`);
            this.b.condBr(cbool, bodies[i], [], next_test, []);
            this.b.sealBlock(next_test);
            this.b.setInsertPoint(next_test);
        }
        // no test matched: default body, or out
        this.b.br(defaultIdx >= 0 ? bodies[defaultIdx] : exit, []);

        // bodies, in document order, falling through to the next
        this.breakTargets.push(exit);
        for (let i = 0; i < n.cases.length; i++) {
            // all of bodies[i]'s preds exist now: its test edge (above) and
            // the fallthrough branch emitted for bodies[i-1] last iteration
            this.b.sealBlock(bodies[i]);
            this.b.setInsertPoint(bodies[i]);
            for (let s of n.cases[i].consequent) {
                this.stmt(s);
                if (this.b.cur.terminated) break;
            }
            if (!this.b.cur.terminated)
                this.b.br(i + 1 < n.cases.length ? bodies[i + 1] : exit, []);
        }
        this.breakTargets.pop();
        this.b.sealBlock(exit);
        this.b.setInsertPoint(exit);
    }

    // lower fresh copies of the finalizers from index `from` (outermost of
    // the crossed set) inward... actually innermost-first: contexts at
    // indexes [from..top] are crossed; run top..from.  each copy runs with
    // the crossed contexts (and their unwind handlers) removed, so a
    // return/break inside a finalizer overrides control per spec, and an
    // exception during the copy propagates without re-running it.
    // returns true if a finalizer terminated the current block.
    runFinalizers(from) {
        let savedCtx = this.finallyCtx;
        let savedHandlers = this.b.handlers;
        for (let i = savedCtx.length - 1; i >= from; i--) {
            this.finallyCtx = savedCtx.slice(0, i);
            this.b.handlers = savedHandlers.slice(0, savedCtx[i].handlerDepth);
            this.stmt(savedCtx[i].node);
            if (this.b.cur.terminated) {
                this.finallyCtx = savedCtx;
                this.b.handlers = savedHandlers;
                return true;
            }
        }
        this.finallyCtx = savedCtx;
        this.b.handlers = savedHandlers;
        return false;
    }

    tryStmt(n) {
        if (n.finalizer) return this.tryFinallyStmt(n);
        let handler = n.handlers[0];
        let catch_bb = this.b.newCatchBlock("catch");
        let join_bb = this.b.newBlock("try_join");

        this.b.pushHandler(catch_bb);
        this.stmt(n.block);
        this.b.popHandler();
        if (!this.b.cur.terminated) this.b.br(join_bb, []);
        this.b.sealBlock(catch_bb);

        this.b.setInsertPoint(catch_bb);
        if (handler.param) {
            let binding = this.analysis.resolve(handler.param);
            this.writeBinding(binding, catch_bb.params[0]);
        }
        this.stmt(handler.body);
        if (!this.b.cur.terminated) this.b.br(join_bb, []);
        this.b.sealBlock(join_bb);
        this.b.setInsertPoint(join_bb);
    }

    // try/finally via finalizer duplication: one copy on the normal path,
    // one in a synthetic catch that rethrows, and copies at each abrupt
    // exit site (see runFinalizers).
    tryFinallyStmt(n) {
        let handler = n.handlers && n.handlers.length > 0 ? n.handlers[0] : null;
        let fin_catch = this.b.newCatchBlock("finally_catch");
        let join_bb = this.b.newBlock("finally_join");

        this.finallyCtx.push({
            node: n.finalizer,
            breakDepth: this.breakTargets.length,
            continueDepth: this.continueTargets.length,
            handlerDepth: this.b.handlers.length,
        });
        this.b.pushHandler(fin_catch);

        if (handler) {
            let catch_bb = this.b.newCatchBlock("catch");
            let inner_join = this.b.newBlock("catch_join");
            this.b.pushHandler(catch_bb);
            this.stmt(n.block);
            this.b.popHandler();
            if (!this.b.cur.terminated) this.b.br(inner_join, []);
            this.b.sealBlock(catch_bb);
            this.b.setInsertPoint(catch_bb);
            if (handler.param) {
                let binding = this.analysis.resolve(handler.param);
                this.writeBinding(binding, catch_bb.params[0]);
            }
            this.stmt(handler.body);
            if (!this.b.cur.terminated) this.b.br(inner_join, []);
            this.b.sealBlock(inner_join);
            this.b.setInsertPoint(inner_join);
        } else {
            this.stmt(n.block);
        }

        this.b.popHandler();
        this.finallyCtx.pop();

        // normal-completion copy
        if (!this.b.cur.terminated) {
            this.stmt(n.finalizer);
            if (!this.b.cur.terminated) this.b.br(join_bb, []);
        }

        // exceptional copy: finalizer, then rethrow
        this.b.sealBlock(fin_catch);
        this.b.setInsertPoint(fin_catch);
        let exc = fin_catch.params[0];
        this.stmt(n.finalizer);
        if (!this.b.cur.terminated) this.b.throwValue(exc);

        this.b.sealBlock(join_bb);
        this.b.setInsertPoint(join_bb);
    }

    finish() {
        if (!this.b.cur.terminated) this.b.ret(this.b.constUndefined());
        return this.b.finish();
    }
}

// lower one analyzed function (and, transitively, function declarations /
// expressions inside it) into `module`.
export function lowerAnalyzedFunction(info, analysis, module, mod_ctx) {
    return lowerOneFunction(info, analysis, module, mod_ctx);
}

function lowerOneFunction(info, analysis, module, mod_ctx) {
    if (info.lowered) return info.fn;
    info.lowered = true;
    let lf = new LowerFunction(info, analysis, module, mod_ctx);
    if (info.node.body.type === b.BlockStatement) lf.stmt(info.node.body);
    else lf.b.ret(lf.expr(info.node.body)); // expression-bodied arrow
    info.fn = lf.finish();
    module.addFunction(info.fn);
    // hoisted closures may reference children whose declaration statement
    // was never reached (e.g. behind an early return); every child still
    // needs a body in the module.
    for (let child of info.children) lowerOneFunction(child, analysis, module, mod_ctx);
    return info.fn;
}

// lower a FunctionDeclaration/FunctionExpression AST node into a fresh
// module; returns { module, fn }
export function lowerFunctionNode(n, name) {
    let analysis = new ScopeAnalysis();
    let info = analysis.analyzeFunction(n, name);
    let module = new Module(info.name);
    let fn = lowerOneFunction(info, analysis, module);
    return { module: module, fn: fn };
}

// lower every top-level function declaration in a parsed program
export function lowerProgram(ast, moduleName) {
    let module = new Module(moduleName || "module");
    for (let s of ast.body) {
        if (s.type === b.FunctionDeclaration) {
            let analysis = new ScopeAnalysis();
            let info = analysis.analyzeFunction(s);
            lowerOneFunction(info, analysis, module);
        }
    }
    return module;
}
