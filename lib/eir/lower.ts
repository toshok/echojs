/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// AST -> EIR lowering.
//
// Covers a whitelisted subset of the (desugared) AST; anything else throws
// LowerNotSupported, which compile() reports as a compile error.
//
// Scope resolution (lib/eir/scopes.ts) runs first and decides, per binding:
// SSA local vs. environment slot.  Lowering then emits make_env /
// env_load / env_store / make_closure directly.
//
// Calling convention mirrors the runtime: every function takes
// (%env, %this, ...params).

import { FunctionBuilder } from "./builder";
import { Module, Func, Block, Inst } from "./ir";
import { ScopeAnalysis, compound_assign_ops, Binding, FnInfo, LoopEnv } from "./scopes";
import { LowerNotSupported } from "./errors";
import { eir_intrinsics } from "./intrinsics";
import type * as e from "../estree";
import type { ModuleInfo } from "../module-info";
import type { TypeOracle } from "./oracle";

// --- module-scope interop types (integrate.ts imports these) -----------------

// a module-slot-backed (or const-folded) reference
export interface SlotRef {
    module: string | null; // "%self", a module path, or null for fold-only
    slot: number;
    constval?: e.Literal;
    writable: boolean;
    exotic?: undefined;
    module_info?: undefined;
}

// a namespace import: the module object itself (module_get_exotic);
// member accesses resolve to slot loads at compile time
export interface ExoticRef {
    exotic: string;
    module_info: ModuleInfo;
    writable: boolean;
    module?: undefined;
    slot?: undefined;
    constval?: undefined;
}

export type ModuleRef = SlotRef | ExoticRef;

export interface ModCtx {
    refs: Map<string, ModuleRef>;
    this_module_info?: ModuleInfo | null;
    module_infos?: Map<string, ModuleInfo> | null;
    // Phase 3: the per-module type oracle (null/absent = no typed fast
    // paths, today's lowering exactly) and the module-wide stats the
    // lowered functions accumulate into
    oracle?: TypeOracle | null;
    typed_stats?: { diamonds: number };
}

// an environment-descriptor chain node: a per-iteration loop env or a
// function env (see envForBinding)
type EnvDesc = LoopEnv | FnInfo;

interface ActiveLabel {
    name: string;
    breakBlock: Block;
    continueBlock: Block | null;
    ctxLen: number;
}

interface FinallyCtx {
    // the finalizer block; fresh copies lower at each crossing exit
    node: e.BlockStatement;
    breakDepth: number;
    continueDepth: number;
    handlerDepth: number;
}

// the Phase 3 typed fast path: source operator -> low-tier f64 op
const f64ops: Record<string, string | undefined> = {
    "+": "f64_add",
    "-": "f64_sub",
    "*": "f64_mul",
    "/": "f64_div",
    "<": "f64_lt",
};

const binops: Record<string, string | undefined> = {
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
// the function\'s own id, or "" for anonymous functions — never the
// scope-qualified EIR name
function displayNameOf(childInfo: FnInfo): string {
    return (childInfo.node.id && childInfo.node.id.name) || "";
}

class LowerFunction {
    info: FnInfo; // FnInfo from scope analysis
    analysis: ScopeAnalysis;
    module: Module;
    // toplevel-as-EIR: this function IS the module toplevel; import/
    // export statements lower here, and slot-backed declarations store
    // through module slots instead of local bindings
    isToplevel: boolean;
    // module-scope interop: module-slot references (imports and this
    // module's exports)
    mod_ctx: ModCtx;
    b: FunctionBuilder;
    envParam: Inst;
    thisParam: Inst;
    // break/continue targets.  loops push onto both stacks; switch
    // statements only onto breakTargets (continue passes through a
    // switch to the enclosing loop).
    breakTargets: Block[] = [];
    continueTargets: Block[] = [];
    // labeled targets: LabeledStatement pushes loop labels onto
    // pendingLabels; the loop lowering claims them (activeLabels)
    // against its own exit/continue blocks.  non-loop labels get a
    // synthetic exit block.  ctxLen = finallyCtx.length at label
    // entry, so a labeled exit runs exactly the finalizers entered
    // since the label.
    pendingLabels: string[] = [];
    activeLabels: ActiveLabel[] = [];
    // materialized per-iteration loop envs lexically active at the
    // current lowering position (innermost last).  the current env
    // value of each is tracked as a builder variable ("%loopenv#id"),
    // so per-iteration refreshes flow through SSA/block params like
    // any other variable (envs are ejsvals).
    activeLoopEnvs: LoopEnv[] = [];
    // active try/finally contexts.  abrupt exits (return, break,
    // continue) crossing a finally boundary lower a fresh copy of each
    // crossed finalizer at the exit site (finalizer duplication).
    finallyCtx: FinallyCtx[] = [];
    curEnv: Inst;
    // Phase 3: the module's type oracle (null = no typed fast paths)
    oracle: TypeOracle | null;

    constructor(info: FnInfo, analysis: ScopeAnalysis, module: Module, mod_ctx?: ModCtx) {
        this.info = info;
        this.analysis = analysis;
        this.module = module;
        this.isToplevel = !!info.isToplevel;
        this.mod_ctx = mod_ctx || { refs: new Map() };
        this.oracle = this.mod_ctx.oracle ?? null;

        const paramNames = info.params.map((p) => p.uid);
        this.b = new FunctionBuilder(info.name, ["%env", "%this"].concat(paramNames));
        this.envParam = this.b.fn.entry!.params[0]!;
        this.thisParam = this.b.fn.entry!.params[1]!;
        // `this` reads go through the builder variable "%this" (seeded to
        // the entry param by the builder): a derived constructor's super()
        // call rebinds it (the runtime constructs the object and returns
        // it), and SSA carries the update.  for every other function it
        // collapses to the entry param.

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
            this.writeBinding(info.argumentsBinding!, a);
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
            const dflt = defaults[i];
            if (!dflt) continue;
            const pb = info.params[i]!;
            const cur = this.readBinding(pb);
            let isundef = this.b.emit("strict_eq", [cur, this.b.constUndefined()], {});
            let ubool = this.b.emit("to_boolean", [isundef], {});
            let dflt_bb = this.b.newBlock(`default_${pb.name}`);
            let join_bb = this.b.newBlock(`default_join_${pb.name}`);
            this.b.condBr(ubool, dflt_bb, [], join_bb, []);
            this.b.sealBlock(dflt_bb);
            this.b.setInsertPoint(dflt_bb);
            const dv = this.expr(dflt);
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

    findChildFn(binding: Binding): FnInfo {
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

    levar(le: LoopEnv): string {
        return `%loopenv#${le.id}`;
    }

    // the env value make_closure should capture at the current position
    curEnvValue(): Inst {
        if (this.activeLoopEnvs.length > 0) {
            const le = this.activeLoopEnvs[this.activeLoopEnvs.length - 1]!;
            return this.b.readVariable(this.levar(le), this.b.cur);
        }
        return this.curEnv;
    }

    // the innermost materialized descriptor at f's definition site
    descAtCreation(f: FnInfo): EnvDesc | null {
        let le = f.creationLoopEnv;
        while (le && !le.materialized) le = le.parentCandidate;
        if (le) return le;
        let p = f.parent;
        if (!p) return null;
        if (p.envSize > 0) return p;
        return this.descAtCreation(p);
    }

    // the descriptor whose env value lives in desc's parent slot
    parentDescOf(desc: EnvDesc): EnvDesc | null {
        if (desc.isLoopEnv) {
            // slot 0 holds curEnv at loop entry: the nearest enclosing
            // materialized loop env, else the function env, else the
            // function's creation-site descriptor (== its incoming env)
            let le = desc.parentCandidate;
            while (le && !le.materialized) le = le.parentCandidate;
            if (le) return le;
            if (desc.fnInfo!.envSize > 0) return desc.fnInfo!;
            return this.descAtCreation(desc.fnInfo!);
        }
        // a function env's parent slot holds its incoming env
        return this.descAtCreation(desc);
    }

    // fresh per-iteration env for captured let/const declared in the loop
    // BODY: emitted at the top of the body block each iteration.  their
    // declarations re-execute per pass, so nothing copies forward.
    enterLoopBody(n: e.Node): LoopEnv | null {
        let ble = this.analysis.loopBodyEnvOf(n);
        if (!ble) return null;
        let outer = this.curEnvValue();
        let e = this.b.emit("make_env", [], { size: ble.envSize });
        this.b.emit("env_store", [e, outer], { slot: 0 });
        this.b.writeVariable(this.levar(ble), this.b.cur, e);
        this.activeLoopEnvs.push(ble);
        return ble;
    }

    leaveLoopBody(ble: LoopEnv | null): void {
        if (ble) this.activeLoopEnvs.pop();
    }

    // a loop lowering claims any labels the enclosing LabeledStatement(s)
    // queued, binding them to its own break/continue blocks
    claimPendingLabels(breakBlock: Block, continueBlock: Block | null): number {
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

    releaseLabels(n: number): void {
        while (n-- > 0) this.activeLabels.pop();
    }

    findLabel(name: string, loc: e.SourceLocation | null | undefined): ActiveLabel {
        for (let i = this.activeLabels.length - 1; i >= 0; i--)
            if (this.activeLabels[i]!.name === name) return this.activeLabels[i]!;
        throw LowerNotSupported(`unknown label '${name}'`, loc);
    }

    // the environment holding `binding`, from the current position
    envForBinding(binding: Binding): Inst {
        let target =
            binding.loopEnv && binding.loopEnv.materialized ? binding.loopEnv : binding.fnInfo;

        let desc: EnvDesc | null;
        let env: Inst;
        if (this.activeLoopEnvs.length > 0) {
            const top = this.activeLoopEnvs[this.activeLoopEnvs.length - 1]!;
            desc = top;
            env = this.b.readVariable(this.levar(top), this.b.cur);
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
                throw new Error(
                    `EIR lowering: broken env chain through ${desc.isLoopEnv ? `loopenv#${desc.id}` : desc.name}`
                );
            env = this.b.emit("env_load", [env], { slot: slot });
            desc = this.parentDescOf(desc);
        }
        if (!desc) throw new Error(`EIR lowering: env chain missed ${binding.uid}`);
        return env;
    }

    readBinding(binding: Binding): Inst {
        if (!binding.captured) return this.b.readVariable(binding.uid, this.b.cur);
        let env = this.envForBinding(binding);
        return this.b.emit("env_load", [env], { slot: binding.slot });
    }

    writeBinding(binding: Binding, value: Inst): void {
        if (!binding.captured) {
            this.b.writeVariable(binding.uid, this.b.cur, value);
            return;
        }
        let env = this.envForBinding(binding);
        this.b.emit("env_store", [env, value], { slot: binding.slot });
    }

    // --- expressions ----------------------------------------------------------

    expr(n: e.Expression | e.SpreadElement): Inst {
        switch (n.type) {
            case "Literal":
                return this.literal(n);
            case "Identifier":
                return this.identifier(n);
            case "ThisExpression": {
                // resolved to a binding = an arrow's lexical this (the
                // owner's captured this, read through the env chain)
                let binding = this.analysis.resolve(n);
                if (binding) return this.readBinding(binding);
                return this.b.readVariable("%this", this.b.cur);
            }
            case "BinaryExpression":
                return this.binary(n);
            case "LogicalExpression":
                return this.logical(n);
            case "UnaryExpression":
                return this.unary(n);
            case "AssignmentExpression":
                return this.assignment(n);
            case "UpdateExpression":
                return this.update(n);
            case "TemplateLiteral":
                return this.template(n);
            case "TaggedTemplateExpression":
                return this.taggedTemplate(n);
            case "CallExpression":
                return this.call(n);
            case "NewExpression":
                return this.newExpr(n);
            case "MemberExpression":
                return this.member(n);
            case "ConditionalExpression":
                return this.conditional(n);
            case "FunctionExpression":
            case "ArrowFunctionExpression":
                return this.functionExpr(n);
            case "SequenceExpression": {
                let v: Inst | undefined;
                for (const sub of n.expressions) v = this.expr(sub);
                return v!;
            }
            case "ArrayExpression": {
                // holes must stay holes (forEach etc. skip them; undefined
                // wouldn't be skipped).  written with plain loops: the
                // arrow-based form of this case miscompiled under the
                // legacy pipeline (undistilled; see the phase-3 notes).
                let holes = false;
                for (let el of n.elements) if (!el) holes = true;
                if (!holes) {
                    const elems: Inst[] = [];
                    for (const el of n.elements) elems.push(this.expr(el!));
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
            case "ObjectExpression": {
                let hasAccessors = n.properties.some((p) => p.kind && p.kind !== "init");
                if (hasAccessors) return this.objectWithAccessors(n);
                let hasComputed = n.properties.some(
                    (p) => p.computed || (p.key.type !== "Identifier" && p.key.type !== "Literal")
                );
                let hasProto = n.properties.some((p) => this.isProtoProp(p));
                if (!hasComputed && !hasProto) {
                    const keys: string[] = [];
                    const values: Inst[] = [];
                    for (const p of n.properties) {
                        keys.push(
                            p.key.type === "Identifier"
                                ? p.key.name
                                : String((p.key as e.Literal).value)
                        );
                        values.push(this.expr(p.value as e.Expression));
                    }
                    return this.b.emit("make_object", values, { keys: keys });
                }
                // computed keys or a `__proto__:` definition: empty object
                // + per-property stores in source order (key evaluates
                // before value, per spec)
                let obj = this.b.emit("make_object", [], { keys: [] });
                for (let p of n.properties) {
                    if (this.isProtoProp(p)) {
                        const v = this.expr(p.value as e.Expression);
                        this.b.emit("call_runtime", [obj, v], {
                            name: "object_literal_set_proto",
                        });
                    } else if (!p.computed && (p.key.type === "Identifier" || p.key.type === "Literal")) {
                        const v = this.expr(p.value as e.Expression);
                        this.b.emit("set_prop_atom", [obj, v], {
                            atom: p.key.type === "Identifier" ? p.key.name : String((p.key as e.Literal).value),
                        });
                    } else {
                        let k = this.expr(p.key);
                        const v = this.expr(p.value as e.Expression);
                        this.b.emit("set_prop", [obj, k, v], {});
                    }
                }
                return obj;
            }
            default:
                throw LowerNotSupported(`expression type ${n.type}`, n.loc);
        }
    }

    // `__proto__: expr` in an object literal (non-computed, non-method,
    // non-shorthand, string or identifier key) is a prototype definition,
    // not an own property (B.3.1 / PropertyDefinitionEvaluation)
    isProtoProp(p: e.Property): boolean {
        if (p.computed || p.method || p.shorthand) return false;
        if (p.kind && p.kind !== "init") return false;
        if (p.key.type === "Identifier") return p.key.name === "__proto__";
        return p.key.type === "Literal" && p.key.value === "__proto__";
    }

    // an object literal containing get/set accessors: empty object, then
    // per-property defines in source order.  a non-computed get/set PAIR
    // for one name becomes a single define_accessor (name-keyed — keying
    // by the key AST node is how the class desugar lost getters, bug
    // #14).  computed-key accessors each define separately in source
    // order (their keys are distinct evaluations); the runtime merges
    // the partial descriptors.
    objectWithAccessors(n: e.ObjectExpression): Inst {
        let obj = this.b.emit("make_object", [], { keys: [] });
        const done = new Set<string>();
        for (let i = 0; i < n.properties.length; i++) {
            const p = n.properties[i]!;
            if (p.computed) {
                let key = this.expr(p.key);
                if (p.kind && p.kind !== "init") {
                    const accessor = this.expr(p.value as e.Expression);
                    this.b.emit("define_accessor_computed", [obj, key, accessor], {
                        kind: p.kind,
                    });
                } else {
                    const v = this.expr(p.value as e.Expression);
                    this.b.emit("set_prop", [obj, key, v], {});
                }
                continue;
            }
            if (p.key.type !== "Identifier" && p.key.type !== "Literal")
                throw LowerNotSupported(`accessor object literal key ${p.key.type}`, n.loc);
            if (this.isProtoProp(p)) {
                const v = this.expr(p.value as e.Expression);
                this.b.emit("call_runtime", [obj, v], { name: "object_literal_set_proto" });
                continue;
            }
            const name = p.key.type === "Identifier" ? p.key.name : String((p.key as e.Literal).value);
            if (p.kind && p.kind !== "init") {
                if (done.has(name)) continue; // the pair lowered together
                done.add(name);
                let getter: Inst | null = null;
                let setter: Inst | null = null;
                for (let j = i; j < n.properties.length; j++) {
                    const q = n.properties[j]!;
                    if (q.kind === "init" || q.computed) continue;
                    const qname = q.key.type === "Identifier" ? q.key.name : String((q.key as e.Literal).value);
                    if (qname !== name) continue;
                    if (q.kind === "get") getter = this.expr(q.value as e.Expression);
                    else if (q.kind === "set") setter = this.expr(q.value as e.Expression);
                }
                this.b.emit(
                    "define_accessor",
                    [obj, getter || this.b.constUndefined(), setter || this.b.constUndefined()],
                    { atom: name }
                );
            } else {
                const v = this.expr(p.value as e.Expression);
                this.b.emit("set_prop_atom", [obj, v], { atom: name });
            }
        }
        return obj;
    }

    literal(n: e.Literal): Inst {
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
                    (n.value.ignoreCase ? "i" : "") +
                    (n.value.sticky ? "y" : "") +
                    (n.value.unicode ? "u" : "");
                return this.b.emit("make_regexp", [], {
                    source: n.value.source,
                    flags: flags,
                });
            }
            default:
                throw LowerNotSupported(`literal ${typeof n.value}`, n.loc);
        }
    }

    identifier(n: e.Identifier): Inst {
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
            return this.b.emit("get_global", [], { atom: n.name });
        }
        if (binding.kind === "self")
            throw LowerNotSupported("function self-reference as a value", n.loc);
        return this.readBinding(binding);
    }

    functionExpr(n: e.FunctionExpression | e.ArrowFunctionExpression): Inst {
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

    binary(n: e.BinaryExpression): Inst {
        let op = binops[n.operator];
        if (!op) throw LowerNotSupported(`binary operator ${n.operator}`, n.loc);
        let l = this.expr(n.left);
        let r = this.expr(n.right);
        // Phase 3: born-typed guarded arithmetic.  When the oracle types
        // BOTH operands as exactly {number}, split the same diamond shape
        // logical() uses: has_tag guards -> fast unbox/f64 op/box vs the
        // generic slow op, rejoining in a boxed block param.  Guarded
        // consumption is correct even when the oracle is wrong — the
        // has_tag guards decide at runtime; only code size/speed change.
        const f64op = f64ops[n.operator];
        if (f64op && this.operandIsNumber(n.left) && this.operandIsNumber(n.right))
            return this.numericDiamond(f64op, op, l, r);
        return this.b.emit(op, [l, r], {});
    }

    // Does this operand node type as exactly {number}?  Numeric literals
    // qualify directly: the oracle's mapping policy leaves literals
    // unmapped (glue), so `x + 1` would otherwise never take the fast
    // path.  (A unary +/- on a numeric literal is the parsed form of a
    // signed literal — pre-EIR desugar does not fold it.)  Everything
    // else asks the oracle, and only a pure {number} answer qualifies —
    // not top, and not reassignment-widened unions like number|undefined.
    operandIsNumber(node: e.Expression): boolean {
        if (!this.oracle) return false; // no oracle, no diamonds — today's lowering
        if (node.type === "Literal") return typeof node.value === "number";
        if (
            node.type === "UnaryExpression" &&
            (node.operator === "-" || node.operator === "+") &&
            node.argument.type === "Literal" &&
            typeof (node.argument as e.Literal).value === "number"
        )
            return true;
        const t = this.oracle.typeOfNode(node);
        return t.tags !== "top" && t.tags.size === 1 && t.tags.has("number");
    }

    // has_tag(l) -> has_tag(r) -> fast: unbox both, f64 op, rejoin boxed;
    // any guard failure -> slow: the generic op.  The join param is an
    // ejsval: raw f64/i1 never crosses a block boundary (P2 verifier
    // rule), so f64 results re-box in the fast block and f64_lt's i1
    // branches to boolean-constant edges into the join.
    numericDiamond(f64op: string, genericOp: string, l: Inst, r: Inst): Inst {
        if (this.mod_ctx.typed_stats) this.mod_ctx.typed_stats.diamonds++;

        const guard2_bb = this.b.newBlock("num_guard2");
        const fast_bb = this.b.newBlock("num_fast");
        const slow_bb = this.b.newBlock("num_slow");
        const join_bb = this.b.newBlock("num_join");
        const result = join_bb.addParam("num");

        const t1 = this.b.emit("has_tag", [l], { tag: "number" });
        this.b.condBr(t1, guard2_bb, [], slow_bb, []);
        this.b.sealBlock(guard2_bb);

        this.b.setInsertPoint(guard2_bb);
        const t2 = this.b.emit("has_tag", [r], { tag: "number" });
        this.b.condBr(t2, fast_bb, [], slow_bb, []);
        this.b.sealBlock(fast_bb);
        this.b.sealBlock(slow_bb);

        this.b.setInsertPoint(fast_bb);
        const ua = this.b.emit("unbox_f64", [l], {});
        const ub = this.b.emit("unbox_f64", [r], {});
        const v = this.b.emit(f64op, [ua, ub], {});
        if (f64op === "f64_lt") {
            const t_bb = this.b.newBlock("num_lt_true");
            const f_bb = this.b.newBlock("num_lt_false");
            this.b.condBr(v, t_bb, [], f_bb, []);
            this.b.sealBlock(t_bb);
            this.b.sealBlock(f_bb);
            this.b.setInsertPoint(t_bb);
            this.b.br(join_bb, [this.b.constBool(true)]);
            this.b.setInsertPoint(f_bb);
            this.b.br(join_bb, [this.b.constBool(false)]);
        } else {
            const boxed = this.b.emit("box_f64", [v], {});
            this.b.br(join_bb, [boxed]);
        }

        this.b.setInsertPoint(slow_bb);
        const g = this.b.emit(genericOp, [l, r], {});
        this.b.br(join_bb, [g]);
        this.b.sealBlock(join_bb);

        this.b.setInsertPoint(join_bb);
        return result;
    }

    logical(n: e.LogicalExpression): Inst {
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

    unary(n: e.UnaryExpression): Inst {
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
                // only member expressions (scopes rejected everything else)
                const m = n.argument as e.MemberExpression;
                const obj = this.expr(m.object as e.Expression);
                const key =
                    !m.computed && m.property.type === "Identifier"
                        ? this.b.constAtom(m.property.name)
                        : this.expr(m.property);
                return this.b.emit("delete_prop", [obj, key], {});
            }
            default:
                throw LowerNotSupported(`unary operator ${n.operator}`, n.loc);
        }
    }

    // the one-time declaration store for a slot-backed toplevel binding:
    // unlike writeIdentifier this may store to read-only refs (an exported
    // const's initializer is a legitimate store)
    writeModuleSlotInit(idNode: e.Identifier, value: Inst): void {
        let ref = this.mod_ctx.refs.get(idNode.name);
        if (!ref || ref.module === undefined || ref.slot === undefined || ref.slot < 0)
            throw LowerNotSupported(
                `toplevel declaration of '${idNode.name}' has no slot`,
                idNode.loc
            );
        this.b.emit("module_slot_store", [value], { module: ref.module, slot: ref.slot });
    }

    // store `value` into this module's export slot named `exportName`
    storeExportSlot(exportName: string, value: Inst, loc: e.SourceLocation | null | undefined): void {
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
    writeIdentifier(idNode: e.Identifier, value: Inst): void {
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
            this.b.emit("set_global", [value], { atom: idNode.name });
            return;
        }
        this.writeBinding(binding, value);
    }

    assignment(n: e.AssignmentExpression): Inst {
        const desugared = compound_assign_ops[n.operator];
        const binop = n.operator === "=" || !desugared ? null : binops[desugared];
        if (n.operator !== "=" && !binop)
            throw LowerNotSupported(`assignment operator ${n.operator}`, n.loc);
        if (n.left.type === "Identifier") {
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
        if (n.left.type === "MemberExpression") {
            // evaluate the object (and computed key) exactly once
            const obj = this.expr(n.left.object as e.Expression);
            let atom: string | null = null;
            let key: Inst | null = null;
            if (!n.left.computed && n.left.property.type === "Identifier")
                atom = n.left.property.name;
            else key = this.expr(n.left.property);
            let v: Inst;
            if (binop) {
                const cur =
                    atom !== null
                        ? this.b.emit("get_prop_atom", [obj], { atom: atom })
                        : this.b.emit("get_prop", [obj, key!], {});
                const rhs = this.expr(n.right);
                v = this.b.emit(binop, [cur, rhs], {});
            } else {
                v = this.expr(n.right);
            }
            if (atom !== null) this.b.emit("set_prop_atom", [obj, v], { atom: atom });
            else this.b.emit("set_prop", [obj, key!, v], {});
            return v;
        }
        throw LowerNotSupported(`assignment target ${n.left.type}`, n.loc);
    }

    // ++/--: ToNumber(old value) via unary_plus, then add/sub 1
    update(n: e.UpdateExpression): Inst {
        let one = this.b.constNumber(1);
        let op = n.operator === "++" ? "add" : "sub";
        if (n.argument.type === "Identifier") {
            let cur = this.identifier(n.argument);
            let old = this.b.emit("unary_plus", [cur], {});
            let nv = this.b.emit(op, [old, one], {});
            this.writeIdentifier(n.argument, nv);
            return n.prefix ? nv : old;
        }
        if (n.argument.type === "MemberExpression") {
            const m = n.argument;
            const obj = this.expr(m.object as e.Expression);
            let atom: string | null = null;
            let key: Inst | null = null;
            if (!m.computed && m.property.type === "Identifier") atom = m.property.name;
            else key = this.expr(m.property);
            const cur =
                atom !== null
                    ? this.b.emit("get_prop_atom", [obj], { atom: atom })
                    : this.b.emit("get_prop", [obj, key!], {});
            const old = this.b.emit("unary_plus", [cur], {});
            const nv = this.b.emit(op, [old, one], {});
            if (atom !== null) this.b.emit("set_prop_atom", [obj, nv], { atom: atom });
            else this.b.emit("set_prop", [obj, key!, nv], {});
            return n.prefix ? nv : old;
        }
        throw LowerNotSupported(`update of ${n.argument.type}`, n.loc);
    }

    // untagged template literal: the inlined default handler — zip cooked
    // strings and ToString'ed substitutions with string_concat (matching
    // the legacy handleTemplateDefaultHandlerCall)
    template(n: e.TemplateLiteral): Inst {
        let strval: Inst | null = null;
        const concat = (s: Inst) => {
            if (!strval) strval = s;
            else strval = this.b.emit("call_runtime", [strval, s], { name: "string_concat" });
        };
        for (let i = 0; i < n.quasis.length; i++) {
            const cooked = n.quasis[i]!.value.cooked;
            if (cooked.length !== 0) concat(this.b.constAtom(cooked));
            if (i < n.expressions.length) {
                const sub = this.expr(n.expressions[i]!);
                concat(this.b.emit("call_runtime", [sub], { name: "ToString" }));
            }
        }
        return strval || this.b.constAtom("");
    }

    // tag`lit ${x}` -> tag(callsite, x): the callsite object is a
    // per-site cached frozen array (template_callsite); member tags keep
    // their receiver as `this`, like any method call
    taggedTemplate(n: e.TaggedTemplateExpression): Inst {
        let callsite = this.b.emit("template_callsite", [], {
            cooked: n.quasi.quasis.map((q) => q.value.cooked),
            raw: n.quasi.quasis.map((q) => q.value.raw),
        });
        const subs = n.quasi.expressions.map((sub) => this.expr(sub));

        let callee: Inst, thisArg: Inst;
        if (n.tag.type === "MemberExpression") {
            thisArg = this.expr(n.tag.object as e.Expression);
            if (!n.tag.computed && n.tag.property.type === "Identifier")
                callee = this.b.emit("get_prop_atom", [thisArg], { atom: n.tag.property.name });
            else {
                let key = this.expr(n.tag.property);
                callee = this.b.emit("get_prop", [thisArg, key], {});
            }
        } else {
            callee = this.expr(n.tag);
            thisArg = this.b.constUndefined();
        }
        return this.b.emit("call", [callee, thisArg, callsite].concat(subs), {});
    }

    // `ns.member` where ns is a namespace import of a JS module resolves
    // to a slot load at compile time (mirroring new-cc's rewrite): the
    // module object doesn't answer runtime property lookups for its
    // exports.  native ("@...") modules DO — they keep the runtime path.
    // returns the loaded value, or null if this isn't such an access.
    exoticMemberLoad(n: e.MemberExpression): Inst | null {
        if (n.object.type !== "Identifier") return null;
        let binding = this.analysis.resolve(n.object);
        if (binding !== null && binding !== undefined) return null; // shadowed
        let ref = this.mod_ctx.refs.get(n.object.name);
        if (!ref || ref.exotic === undefined || !ref.module_info) return null;
        if (ref.exotic[0] === "@") return null; // native: runtime lookup works
        let name = null;
        if (!n.computed && n.property.type === "Identifier") name = n.property.name;
        else if (n.property.type === "Literal" && typeof n.property.value === "string")
            name = n.property.value;
        if (name === null) return null;
        let export_info = ref.module_info.exports.get(name);
        if (!export_info || export_info.promoted) return null; // promoted slots are private
        let cv = export_info.constval;
        if (cv && cv.type === "Literal" && (cv.value === null || typeof cv.value !== "object"))
            return this.literal(cv);
        return this.b.emit("module_slot_load", [], {
            module: ref.exotic,
            slot: export_info.slot_num,
        });
    }

    member(n: e.MemberExpression): Inst {
        let slotv = this.exoticMemberLoad(n);
        if (slotv) return slotv;
        let obj = this.expr(n.object);
        if (!n.computed && n.property.type === "Identifier")
            return this.b.emit("get_prop_atom", [obj], { atom: n.property.name });
        let key = this.expr(n.property);
        return this.b.emit("get_prop", [obj, key], {});
    }

    call(n: e.CallExpression): Inst {
        // %-intrinsic calls from the pre-EIR desugar passes lower through
        // the table in intrinsics.js (scopes.js already rejected unknowns)
        if (n.callee.type === "Identifier" && n.callee.name[0] === "%")
            return this.intrinsicCall(n);
        let callee, thisArg;
        if (n.callee.type === "MemberExpression") {
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
            if (!n.callee.computed && n.callee.property.type === "Identifier")
                callee = this.b.emit("get_prop_atom", [thisArg], {
                    atom: n.callee.property.name,
                });
            else {
                let key = this.expr(n.callee.property);
                callee = this.b.emit("get_prop", [thisArg, key], {});
            }
        } else {
            // direct calls: recursion through the self binding skips
            // closure dispatch
            if (n.callee.type === "Identifier") {
                let binding = this.analysis.resolve(n.callee);
                if (binding && binding.kind === "self" && binding.fnInfo === this.info) {
                    let dthis = this.b.constUndefined();
                    let dargs = n.arguments.map((a) => this.expr(a));
                    return this.b.emit("call", [this.envParam, dthis].concat(dargs), {
                        direct: this.info.name,
                    });
                }
            }
            callee = this.expr(n.callee);
            thisArg = this.b.constUndefined();
        }
        let args = n.arguments.map((a) => this.expr(a));
        return this.b.emit("call", [callee, thisArg].concat(args), {});
    }

    intrinsicCall(n: e.CallExpression): Inst {
        const calleeName = (n.callee as e.Identifier).name;
        const intr = eir_intrinsics[calleeName];
        if (!intr) throw LowerNotSupported(`intrinsic ${calleeName}`, n.loc);
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

    newExpr(n: e.NewExpression): Inst {
        let callee = this.expr(n.callee);
        let args = n.arguments.map((a) => this.expr(a));
        return this.b.emit("construct", [callee].concat(args), {});
    }

    conditional(n: e.ConditionalExpression): Inst {
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

    stmt(n: e.Statement): void {
        switch (n.type) {
            case "BlockStatement":
                for (let s of n.body) {
                    this.stmt(s);
                    if (this.b.cur.terminated) return;
                }
                return;
            case "VariableDeclaration":
                for (let d of n.declarations) {
                    if (d.id.type === "ObjectPattern") {
                        this.lowerObjectPatternDecl(d);
                        continue;
                    }
                    if (d.id.type !== "Identifier")
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
                    this.writeBinding(binding!, this.b.constUndefined());
                    const init = d.init ? this.expr(d.init) : this.b.constUndefined();
                    this.writeBinding(binding!, init);
                }
                return;
            case "FunctionDeclaration": {
                let binding = this.analysis.resolve(n.id);
                if (!binding && this.isToplevel) {
                    // a slot-backed module function: lower it, then store
                    // its closure to the slot at this statement's source
                    // position (same hoisting caveat as the legacy
                    // %moduleSetSlot rewrite)
                    const childInfo = this.analysis.infoFor(n)!;
                    lowerOneFunction(childInfo, this.analysis, this.module, this.mod_ctx);
                    let closure = this.b.emit("make_closure", [this.curEnvValue()], {
                        fn: childInfo.name,
                        name: displayNameOf(childInfo),
                    });
                    this.writeModuleSlotInit(n.id, closure);
                    return;
                }
                // closure was created (hoisted) at entry; lower the body now
                lowerOneFunction(this.analysis.infoFor(n)!, this.analysis, this.module, this.mod_ctx);
                return;
            }
            case "ImportDeclaration":
                if (!this.isToplevel) throw LowerNotSupported("import declaration", n.loc);
                // module resolution happens in the toplevel scaffolding;
                // a bare `import "m"` also touches the module object for
                // parity with the legacy %moduleGetExotic rewrite
                if (n.specifiers.length === 0)
                    this.b.emit("module_get_exotic", [], { module: n.source_path!.value });
                return;
            case "ExportNamedDeclaration": {
                if (!this.isToplevel) throw LowerNotSupported("export declaration", n.loc);
                if (n.declaration && !Array.isArray(n.declaration)) return this.stmt(n.declaration);
                // export { a as b } from "m": copy the source module's
                // slots into ours at init time (matching the legacy
                // moduleGetSlot/moduleSetSlot rewrite — a snapshot, not a
                // live binding)
                if (n.source) {
                    const source = n.source_path!.value;
                    let source_info =
                        this.mod_ctx.module_infos && this.mod_ctx.module_infos.get(source);
                    if (!source_info || source_info.isNative())
                        throw LowerNotSupported(`re-export from '${source}'`, n.loc);
                    for (let spec of n.specifiers) {
                        let export_info = source_info.exports.get(spec.local.name);
                        if (!export_info || export_info.promoted)
                            throw LowerNotSupported(
                                `module '${source}' doesn't export '${spec.local.name}'`,
                                n.loc
                            );
                        let v = this.b.emit("module_slot_load", [], {
                            module: source,
                            slot: export_info.slot_num,
                        });
                        this.storeExportSlot(spec.exported.name, v, n.loc);
                    }
                    return;
                }
                // export { A, B as C }: copy the locals' current values
                // into the exported slots at this statement's position
                for (let spec of n.specifiers) {
                    let v = this.identifier(spec.local);
                    this.storeExportSlot(spec.exported.name, v, n.loc);
                }
                return;
            }
            case "ExportDefaultDeclaration": {
                if (!this.isToplevel) throw LowerNotSupported("export default", n.loc);
                const v = this.expr(n.declaration as e.Expression);
                this.storeExportSlot("default", v, n.loc);
                return;
            }
            case "ExpressionStatement":
                this.expr(n.expression);
                return;
            case "IfStatement":
                return this.ifStmt(n);
            case "WhileStatement":
                return this.whileStmt(n);
            case "DoWhileStatement":
                return this.doWhileStmt(n);
            case "ForStatement":
                return this.forStmt(n);
            case "ForOfStatement":
                return this.forOfStmt(n);
            case "ForInStatement":
                return this.forInStmt(n);
            case "SwitchStatement":
                return this.switchStmt(n);
            case "ReturnStatement": {
                let rv = n.argument ? this.expr(n.argument) : this.b.constUndefined();
                if (this.finallyCtx.length > 0) {
                    if (this.runFinalizers(0)) return; // a finalizer overrode control
                }
                this.b.ret(rv);
                return;
            }
            case "ThrowStatement":
                this.b.throwValue(this.expr(n.argument));
                return;
            case "TryStatement":
                return this.tryStmt(n);
            case "LabeledStatement": {
                // labels on loops bind to the loop's own blocks (the loop
                // lowering claims them); labels on anything else get a
                // synthetic exit block for labeled breaks
                let body = n.body;
                while (body.type === "LabeledStatement") body = body.body;
                let isLoop =
                    body.type === "WhileStatement" ||
                    body.type === "DoWhileStatement" ||
                    body.type === "ForStatement" ||
                    body.type === "ForInStatement" ||
                    body.type === "ForOfStatement";
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
            case "BreakStatement": {
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
                this.b.br(this.breakTargets[targetLen - 1]!, []);
                return;
            }
            case "ContinueStatement": {
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
                this.b.br(this.continueTargets[targetLen - 1]!, []);
                return;
            }
            case "EmptyStatement":
            case "DebuggerStatement": // a no-op in compiled code
                return;
            default:
                throw LowerNotSupported(`statement type ${n.type}`, n.loc);
        }
    }

    ifStmt(n: e.IfStatement): void {
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
            this.stmt(n.alternate!);
            if (!this.b.cur.terminated) this.b.br(join_bb, []);
        }
        this.b.sealBlock(join_bb);
        this.b.setInsertPoint(join_bb);
    }

    whileStmt(n: e.WhileStatement): void {
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

    doWhileStmt(n: e.DoWhileStatement): void {
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

    forStmt(n: e.ForStatement): void {
        // captured let/const loop vars live in a fresh env per iteration:
        // the initial env is created before the init declaration runs, and
        // each pass through the update block makes a new env, copying the
        // loop vars forward (so the update and next test see the copies,
        // and closures made in earlier iterations keep their own)
        let le = this.analysis.loopEnvOf(n);
        let outerEnvVal: Inst | null = null;
        if (le) {
            outerEnvVal = this.curEnvValue();
            let e = this.b.emit("make_env", [], { size: le.envSize });
            this.b.emit("env_store", [e, outerEnvVal!], { slot: 0 });
            this.b.writeVariable(this.levar(le), this.b.cur, e);
            this.activeLoopEnvs.push(le);
        }

        if (n.init) {
            if (n.init.type === "VariableDeclaration") this.stmt(n.init);
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
            this.b.emit("env_store", [enew, outerEnvVal!], { slot: 0 });
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

    lowerObjectPatternDecl(d: e.VariableDeclarator): void {
        const src = d.init ? this.expr(d.init) : this.b.constUndefined();
        for (const prop of (d.id as e.ObjectPattern).properties) {
            const keyName =
                prop.key.type === "Identifier"
                    ? prop.key.name
                    : String((prop.key as e.Literal).value);
            let target = prop.value as e.Pattern;
            let dflt: e.Expression | null = null;
            if (target.type === "AssignmentPattern") {
                dflt = target.right;
                target = target.left;
            }
            const binding = this.analysis.resolve(target)!;
            const v = this.b.emit("get_prop_atom", [src], { atom: keyName });
            this.writeBinding(binding, v);
            if (dflt) {
                let isundef = this.b.emit("strict_eq", [v, this.b.constUndefined()], {});
                let ubool = this.b.emit("to_boolean", [isundef], {});
                let dflt_bb = this.b.newBlock(`pat_default_${keyName}`);
                let join_bb = this.b.newBlock(`pat_join_${keyName}`);
                this.b.condBr(ubool, dflt_bb, [], join_bb, []);
                this.b.sealBlock(dflt_bb);
                this.b.setInsertPoint(dflt_bb);
                const dv = this.expr(dflt);
                this.writeBinding(binding, dv);
                this.b.br(join_bb, []);
                this.b.sealBlock(join_bb);
                this.b.setInsertPoint(join_bb);
            }
        }
    }

    // mirrors the legacy DesugarForOf expansion: iterable[Symbol.iterator]()
    // once, then `next()` per iteration, testing `.done` and binding `.value`
    forOfStmt(n: e.ForOfStatement): void {
        // a captured let/const loop var gets a fresh env each iteration
        // (created at the top of the body, right before the var is bound);
        // no copying between iterations — the binding is (re)assigned from
        // the iteration value anyway.  an initial env exists before the
        // RHS evaluates: scope analysis declares the binding before
        // walking the RHS, so a closure there may already capture it
        // (reading undefined, matching the legacy alloca behavior).
        let le = this.analysis.loopEnvOf(n);
        let outerEnvVal: Inst | null = null;
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
            this.b.emit("env_store", [e, outerEnvVal!], { slot: 0 });
            this.b.writeVariable(this.levar(le), this.b.cur, e);
        }
        const v = this.b.emit("get_prop_atom", [res], { atom: "value" });
        if (n.left.type === "VariableDeclaration") {
            const binding = this.analysis.resolve(n.left.declarations[0]!.id)!;
            this.writeBinding(binding, v);
        } else {
            this.writeIdentifier(n.left as e.Identifier, v);
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
    forInStmt(n: e.ForInStatement): void {
        // fresh env per iteration for a captured let/const binding, with
        // an initial env before the RHS evaluates — as in forOfStmt
        let le = this.analysis.loopEnvOf(n);
        let outerEnvVal: Inst | null = null;
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
            this.b.emit("env_store", [e, outerEnvVal!], { slot: 0 });
            this.b.writeVariable(this.levar(le), this.b.cur, e);
        }
        const v = this.b.emit("prop_iter_current", [iter], {});
        if (n.left.type === "VariableDeclaration") {
            const binding = this.analysis.resolve(n.left.declarations[0]!.id)!;
            this.writeBinding(binding, v);
        } else {
            this.writeIdentifier(n.left as e.Identifier, v);
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

    switchStmt(n: e.SwitchStatement): void {
        let disc = this.expr(n.discriminant);
        let exit = this.b.newBlock("switch_exit");
        let bodies = n.cases.map((c, i) => this.b.newBlock(`case_body${i}`));
        let defaultIdx = n.cases.findIndex((c) => !c.test);

        // test chain, in document order, skipping default
        for (let i = 0; i < n.cases.length; i++) {
            const test = n.cases[i]!.test;
            if (!test) continue;
            const tv = this.expr(test);
            const cmp = this.b.emit("strict_eq", [disc, tv], {});
            const cbool = this.b.emit("to_boolean", [cmp], {});
            const next_test = this.b.newBlock(`case_test${i}`);
            this.b.condBr(cbool, bodies[i]!, [], next_test, []);
            this.b.sealBlock(next_test);
            this.b.setInsertPoint(next_test);
        }
        // no test matched: default body, or out
        this.b.br(defaultIdx >= 0 ? bodies[defaultIdx]! : exit, []);

        // bodies, in document order, falling through to the next
        this.breakTargets.push(exit);
        for (let i = 0; i < n.cases.length; i++) {
            // all of bodies[i]'s preds exist now: its test edge (above) and
            // the fallthrough branch emitted for bodies[i-1] last iteration
            this.b.sealBlock(bodies[i]!);
            this.b.setInsertPoint(bodies[i]!);
            for (const s of n.cases[i]!.consequent) {
                this.stmt(s);
                if (this.b.cur.terminated) break;
            }
            if (!this.b.cur.terminated)
                this.b.br(i + 1 < n.cases.length ? bodies[i + 1]! : exit, []);
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
    runFinalizers(from: number): boolean {
        let savedCtx = this.finallyCtx;
        let savedHandlers = this.b.handlers;
        for (let i = savedCtx.length - 1; i >= from; i--) {
            this.finallyCtx = savedCtx.slice(0, i);
            this.b.handlers = savedHandlers.slice(0, savedCtx[i]!.handlerDepth);
            this.stmt(savedCtx[i]!.node);
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

    tryStmt(n: e.TryStatement): void {
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
        if (handler!.param) {
            const binding = this.analysis.resolve(handler!.param)!;
            this.writeBinding(binding, catch_bb.params[0]!);
        }
        this.stmt(handler!.body);
        if (!this.b.cur.terminated) this.b.br(join_bb, []);
        this.b.sealBlock(join_bb);
        this.b.setInsertPoint(join_bb);
    }

    // try/finally via finalizer duplication: one copy on the normal path,
    // one in a synthetic catch that rethrows, and copies at each abrupt
    // exit site (see runFinalizers).
    tryFinallyStmt(n: e.TryStatement): void {
        let handler = n.handlers && n.handlers.length > 0 ? n.handlers[0] : null;
        let fin_catch = this.b.newCatchBlock("finally_catch");
        let join_bb = this.b.newBlock("finally_join");

        this.finallyCtx.push({
            node: n.finalizer!,
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
                const binding = this.analysis.resolve(handler.param)!;
                this.writeBinding(binding, catch_bb.params[0]!);
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
            this.stmt(n.finalizer!);
            if (!this.b.cur.terminated) this.b.br(join_bb, []);
        }

        // exceptional copy: finalizer, then rethrow
        this.b.sealBlock(fin_catch);
        this.b.setInsertPoint(fin_catch);
        const exc = fin_catch.params[0]!;
        this.stmt(n.finalizer!);
        if (!this.b.cur.terminated) this.b.throwValue(exc);

        this.b.sealBlock(join_bb);
        this.b.setInsertPoint(join_bb);
    }

    finish(): Func {
        if (!this.b.cur.terminated) this.b.ret(this.b.constUndefined());
        return this.b.finish();
    }
}

// lower one analyzed function (and, transitively, function declarations /
// expressions inside it) into `module`.
export function lowerAnalyzedFunction(info: FnInfo, analysis: ScopeAnalysis, module: Module, mod_ctx?: ModCtx): Func {
    return lowerOneFunction(info, analysis, module, mod_ctx);
}

function lowerOneFunction(info: FnInfo, analysis: ScopeAnalysis, module: Module, mod_ctx?: ModCtx): Func {
    if (info.lowered) return info.fn!;
    info.lowered = true;
    let lf = new LowerFunction(info, analysis, module, mod_ctx);
    if (info.node.body.type === "BlockStatement") lf.stmt(info.node.body);
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
export function lowerFunctionNode(
    n: e.Function,
    name?: string,
    oracle?: TypeOracle | null
): { module: Module; fn: Func; diamonds: number } {
    let analysis = new ScopeAnalysis();
    let info = analysis.analyzeFunction(n, name);
    let module = new Module(info.name);
    let typed_stats = { diamonds: 0 };
    let fn = lowerOneFunction(info, analysis, module, {
        refs: new Map(),
        oracle: oracle ?? null,
        typed_stats,
    });
    return { module: module, fn: fn, diamonds: typed_stats.diamonds };
}

// lower every top-level function declaration in a parsed program
export function lowerProgram(ast: e.Program, moduleName?: string): Module {
    let module = new Module(moduleName || "module");
    for (let s of ast.body) {
        if (s.type === "FunctionDeclaration") {
            let analysis = new ScopeAnalysis();
            let info = analysis.analyzeFunction(s);
            lowerOneFunction(info, analysis, module);
        }
    }
    return module;
}
