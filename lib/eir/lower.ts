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
import type { ShapeField } from "./ir";
import { ScopeAnalysis, compound_assign_ops, Binding, FnInfo, LoopEnv } from "./scopes";
import { LowerNotSupported } from "./errors";
import { eir_intrinsics } from "./intrinsics";
import type * as e from "../estree";
import type { ModuleInfo } from "../module-info";
import type { TypeOracle } from "./oracle";
import { passes } from "../pass-config";

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
    // class-this shape facts: class evidence (ctor fn node or the
    // declared-field-names array) -> birth-shape fields (null = does
    // not qualify), computed once per module (classBirthShape)
    class_shapes?: Map<object, ShapeField[] | null>;
    refs: Map<string, ModuleRef>;
    this_module_info?: ModuleInfo | null;
    module_infos?: Map<string, ModuleInfo> | null;
    // the per-module type oracle (null/absent = no typed fast
    // paths, today's lowering exactly) and the module-wide stats the
    // lowered functions accumulate into.  the shape-guard lowering adds the shape
    // telemetry: sites = atom property accesses that consulted the oracle,
    // guards = shape diamonds emitted, declined = counted reasons
    // (promotion criterion 5 — visible degradation).
    oracle?: TypeOracle | null;
    typed_stats?: {
        diamonds: number;
        trusted?: number;
        shape_sites?: number;
        shape_guards?: number;
        // sites guarded with the 2-way polymorphic
        // chain (a subset of shape_guards)
        shape_poly_guards?: number;
        shape_declined?: Record<string, number>;
        // born-with-shape telemetry — literal sites
        // batched into make_object_shaped, constructor prefixes batched
        // into fill_object_shaped diamonds, and counted fence declines
        born_shaped?: number;
        ctor_fills?: number;
        fence_declined?: Record<string, number>;
        // typed (raw f64) slot accesses emitted
        typed_loads?: number;
        typed_stores?: number;
        // construct sites virtualized by the optimizer's
        // epoch-guarded constructor-result sinking
        ctor_sunk?: number;
    };
    // --types-dump: per-site shape census lines
    shape_dump?: boolean;
    // script-goal semantics (--script): toplevel `this` is globalThis
    // instead of the module goal's undefined
    script?: boolean;
}

// clone-lowering mode (specialize.ts).  The clone gets an
// unboxed signature (f64 formals, boxed once at entry); `trusted`
// selects how the body consumes the oracle:
//   - trusted: oracle-number arithmetic lowers UNGUARDED — no diamonds,
//     no slow paths.  This is the deliberate unguarded-consumption
//     line: oracle claims become facts, backed by the differential
//     harness and by the escape analysis that restricts trusted clones
//     to functions whose every runtime call the analysis covered.
//   - untrusted (the export-boundary wrapper's clone): the
//     body keeps the ordinary guarded diamonds — the oracle is never
//     consumed as fact, because the clone is entered from escaping
//     entry points whose callers the analysis did NOT see (maam's
//     constant-propagation domain may have pruned branches under
//     call-site constants, so even all-number external arguments can
//     escape its claims).  The f64 formals are boxed once at entry;
//     box_f64 is the optimizer's structural number proof, so
//     formal-rooted diamonds fold trust-free.  The diamond gate widens
//     to assume-and-guard (see operandPlausiblyNumber).
export interface SpecMode {
    cloneName: string;
    trusted: boolean;
    // formal parameter types; "f64" formals arrive raw and are boxed once
    // at entry
    formals: ("any" | "f64")[];
    // when "f64" (trusted clones only), `return <expr>` with an
    // oracle-number argument returns the raw f64 (unguarded unbox); any
    // other return shape survives to the structural post-check in
    // specialize.ts, which discards the clone
    result: "any" | "f64";
}

// the runtime's shaped field-count ceiling
// (EJS_SHAPE_FIELD_CAP_MAX in runtime/ejs-shapes.h) — born-shaped sites
// beyond it would only ever take the runtime's sequential fallback, so
// they keep today's lowering
const EJS_SHAPE_FIELD_CAP_MAX = 14;

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

// the typed fast path: source operator -> low-tier f64 op
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
    "**": "exp",
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
    // ejs_display_name carries the spec .name when it differs from the
    // id (class methods' ids are qualified LLVM names, NamedEvaluation
    // names anonymous functions after their binding/property)
    const display = (childInfo.node as unknown as Record<string, unknown>)["ejs_display_name"];
    if (typeof display === "string") return display;
    return (childInfo.node.id && childInfo.node.id.name) || "";
}

// the function's spec .length.  The parser records it before the desugar
// passes rewrite param lists (`ejs_fn_length`); synthesized functions
// (and the esprima fallback) get the count of leading no-default,
// non-rest formals as seen here.
function specFnLength(n: e.Function): number {
    const recorded = (n as unknown as Record<string, unknown>)["ejs_fn_length"];
    if (typeof recorded === "number") return recorded;
    let count = 0;
    for (let i = 0; i < n.params.length; i++) {
        if (n.params[i]!.type === "RestElement" || (n.defaults && n.defaults[i]) != null) break;
        count++;
    }
    return count;
}

// does a `this.<name> = ...` store OUTSIDE `allowed` (or any computed
// `this[e] = ...` store) appear in these statements — including arrow
// bodies, which share the enclosing `this`?  Used to reject class-this
// birth shapes the constructor could extend: a store to an allowed
// (already-defined) field is a plain set and leaves the shape alone.
// Plain nested functions bind their own `this` and are skipped.
function thisStoreOutside(stmts: e.Statement[], allowed: ReadonlySet<string>): boolean {
    let found = false;
    const walk = (n: unknown): void => {
        if (found || n === null || typeof n !== "object") return;
        if (Array.isArray(n)) {
            for (const c of n) walk(c);
            return;
        }
        const node = n as { type?: string } & Record<string, unknown>;
        if (typeof node.type !== "string") return;
        if (node.type === "FunctionDeclaration" || node.type === "FunctionExpression") return;
        if (node.type === "AssignmentExpression") {
            const left = node["left"] as
                | { type?: string; computed?: boolean; object?: { type?: string }; property?: { type?: string; name?: string } }
                | undefined;
            if (left?.type === "MemberExpression" && left.object?.type === "ThisExpression") {
                if (
                    left.computed ||
                    left.property?.type !== "Identifier" ||
                    !allowed.has(left.property.name!)
                ) {
                    found = true;
                    return;
                }
            }
        }
        for (const k of Object.keys(node)) {
            if (k === "loc" || k === "range") continue;
            walk(node[k]);
        }
    };
    walk(stmts);
    return found;
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
    // the module's type oracle (null = no typed fast paths)
    oracle: TypeOracle | null;
    // non-null when lowering a specialized clone
    spec: SpecMode | null;
    // a generator body: yields lower to gen_yield ops and
    // gen-lower.ts rewrites the function into a resume-dispatch state
    // machine after optimization
    isGenBody = false;

    constructor(
        info: FnInfo,
        analysis: ScopeAnalysis,
        module: Module,
        mod_ctx?: ModCtx,
        spec?: SpecMode | null
    ) {
        this.info = info;
        this.analysis = analysis;
        this.module = module;
        this.isToplevel = !!info.isToplevel;
        this.mod_ctx = mod_ctx || { refs: new Map() };
        this.oracle = this.mod_ctx.oracle ?? null;
        this.spec = spec ?? null;

        const paramNames = info.params.map((p) => p.uid);
        this.b = new FunctionBuilder(
            this.spec ? this.spec.cloneName : info.name,
            ["%env", "%this"].concat(paramNames)
        );
        this.isGenBody = !!(info.node as unknown as Record<string, unknown>)["ejs_gen_eir_body"];
        this.b.fn.genBody = this.isGenBody;
        this.envParam = this.b.fn.entry!.params[0]!;
        this.thisParam = this.b.fn.entry!.params[1]!;

        // specialized-clone entry: f64 formals arrive raw and re-enter the
        // boxed world exactly once, right here; the body then lowers
        // against the boxed value like any other binding.  (box_f64 is
        // also the optimizer's value-intrinsic number proof, so any
        // residual guarded diamond over a formal folds.)
        if (this.spec) {
            const entry = this.b.fn.entry!;
            this.b.fn.sig = { formals: this.spec.formals.slice(), result: this.spec.result };
            for (let i = 0; i < info.params.length; i++) {
                if (this.spec.formals[i] !== "f64") continue;
                const p = entry.params[i + 2]!;
                p.type = "f64";
                const boxed = this.b.emit("box_f64", [p], {});
                this.b.writeVariable(info.params[i]!.uid, entry, boxed);
            }
        }
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

        // 9.2.1.2 OrdinaryCallBindThis: sloppy-mode functions replace a
        // null/undefined `this` with the global object.  Only functions
        // that actually read `this` pay for the check, and strict code
        // (all module-goal code) emits nothing.
        if (!this.isToplevel && !info.strict && info.usesThis && !this.spec) {
            const coerced = this.b.emit("sloppy_this", [this.thisParam], {});
            this.b.writeVariable("%this", this.b.fn.entry!, coerced);
            this.thisParam = coerced;
        }

        // an arrow below captures our `this`: store it in the env (kept
        // in sync by intrinsicCall when super() rebinds this).  the
        // toplevel's `this` is undefined under the module goal,
        // globalThis under --script.
        if (info.thisBinding && info.thisBinding.captured) {
            const this_val = this.isToplevel
                ? this.mod_ctx.script
                    ? this.b.emit("get_global", [], { atom: "globalThis", for_typeof: 1 })
                    : this.b.constUndefined()
                : this.thisParam;
            this.writeBinding(info.thisBinding, this_val);
        }

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
                    len: specFnLength(childInfo.node),
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
                // toplevel `this`: undefined under the module goal,
                // globalThis under --script
                if (this.isToplevel) {
                    if (this.mod_ctx.script)
                        return this.b.emit("get_global", [], { atom: "globalThis", for_typeof: 1 });
                    return this.b.constUndefined();
                }
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
                // SpreadElement properties were desugared by DesugarSpread
                for (const p of n.properties)
                    if (p.type === "SpreadElement")
                        throw LowerNotSupported("object spread survived desugaring", n.loc);
                const props = n.properties as e.Property[];
                let hasAccessors = props.some((p) => p.kind && p.kind !== "init");
                if (hasAccessors) return this.objectWithAccessors(n);
                let hasComputed = props.some(
                    (p) => p.computed || (p.key.type !== "Identifier" && p.key.type !== "Literal")
                );
                let hasProto = props.some((p) => this.isProtoProp(p));
                if (!hasComputed && !hasProto) {
                    const keys: string[] = [];
                    const values: Inst[] = [];
                    for (const p of props) {
                        keys.push(
                            p.key.type === "Identifier"
                                ? p.key.name
                                : String((p.key as e.Literal).value)
                        );
                        values.push(this.expr(p.value as e.Expression));
                    }
                    // a statically-keyed literal is born
                    // with its shape — key order and count are the site's
                    // static truth, no oracle fact needed (the runtime
                    // derives true reprs from the actual values and falls
                    // back to sequential sets off the shaped fast path).
                    // NOT --types-gated: without the oracle the
                    // static reprs are simply all-boxed; the runtime's
                    // birth derivation supplies the true ones, and the
                    // single-cell embedded allocation applies to flag-off
                    // literals exactly as to typed ones.
                    if (
                        passes().bornShaped &&
                        keys.length >= 1 &&
                        keys.length <= EJS_SHAPE_FIELD_CAP_MAX &&
                        new Set(keys).size === keys.length &&
                        keys.every((k) => !/^[0-9]/.test(k))
                    ) {
                        const fields: ShapeField[] = props.map((p, i) => ({
                            name: keys[i]!,
                            repr: this.operandIsNumber(p.value as e.Expression)
                                ? ("f64" as const)
                                : ("boxed" as const),
                        }));
                        const key = this.module.internShape(fields);
                        const stats = this.mod_ctx.typed_stats;
                        if (stats) stats.born_shaped = (stats.born_shaped ?? 0) + 1;
                        return this.b.emit("make_object_shaped", values, { shape: key });
                    }
                    return this.b.emit("make_object", values, { keys: keys });
                }
                // computed keys or a `__proto__:` definition: empty object
                // + per-property stores in source order (key evaluates
                // before value, per spec)
                let obj = this.b.emit("make_object", [], { keys: [] });
                for (let p of props) {
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
        // only reached from the ObjectExpression case, after the
        // no-SpreadElement assert
        const props = n.properties as e.Property[];
        let obj = this.b.emit("make_object", [], { keys: [] });
        const done = new Set<string>();
        for (let i = 0; i < props.length; i++) {
            const p = props[i]!;
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
                for (let j = i; j < props.length; j++) {
                    const q = props[j]!;
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
        // a regex literal: fresh RegExp per evaluation.  Checked before
        // the null-literal case, and lowered from the syntactic n.regex:
        // n.value is acorn's host-constructed RegExp, which is null
        // whenever the COMPILER's engine rejects the pattern — a /v
        // literal used to fall through to constNull here and evaluate
        // to null (and rebuilding flags from value's booleans would
        // drop any flag that engine predates).  The runtime's own
        // RegExp throws a SyntaxError at evaluation if the pattern is
        // bad.
        if (n.regex !== undefined && n.regex !== null) {
            return this.b.emit("make_regexp", [], {
                source: n.regex.pattern,
                flags: n.regex.flags,
            });
        }
        if (n.value === null) return this.b.constNull();
        switch (typeof n.value) {
            case "number":
                return this.b.constNumber(n.value);
            case "string":
                return this.b.constAtom(n.value);
            case "boolean":
                return this.b.constBool(n.value);
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
            len: specFnLength(childInfo.node),
        });
    }

    binary(n: e.BinaryExpression): Inst {
        let op = binops[n.operator];
        if (!op) throw LowerNotSupported(`binary operator ${n.operator}`, n.loc);
        let l = this.expr(n.left as e.Expression);
        let r = this.expr(n.right);
        // born-typed guarded arithmetic.  When the oracle types
        // BOTH operands as exactly {number}, split the same diamond shape
        // logical() uses: has_tag guards -> fast unbox/f64 op/box vs the
        // generic slow op, rejoining in a boxed block param.  Guarded
        // consumption is correct even when the oracle is wrong — the
        // has_tag guards decide at runtime; only code size/speed change.
        const f64op = f64ops[n.operator];
        if (f64op && this.operandIsNumber(n.left as e.Expression) && this.operandIsNumber(n.right)) {
            // trusted-clone bodies consume the oracle UNGUARDED: no
            // diamond, no slow path — unbox, compute, re-box.  Everywhere
            // else the guarded diamond stands.
            if (this.spec && this.spec.trusted) return this.trustedNumeric(f64op, l, r);
            return this.numericDiamond(f64op, op, l, r);
        }
        // untrusted (wrapper) clone bodies assume-and-guard: the diamond
        // is correct for ANY operand values, so a plausibly-number claim
        // (not provably non-number — incl. nodes the oracle never saw,
        // the norm for an exported-but-never-called-internally function)
        // is enough to justify emitting it.  The entry box_f64 proofs
        // fold the formal-rooted ones; the rest keep their slow paths.
        if (
            f64op &&
            this.spec &&
            !this.spec.trusted &&
            this.operandPlausiblyNumber(n.left as e.Expression) &&
            this.operandPlausiblyNumber(n.right)
        )
            return this.numericDiamond(f64op, op, l, r);
        return this.b.emit(op, [l, r], {});
    }

    // unguarded typed arithmetic (clone lowering only): unbox both
    // operands, apply the f64 op, and re-enter the boxed world.  f64_lt's
    // i1 rejoins as boxed booleans through the same constant-edge shape
    // the diamond's fast arm uses (i1 never crosses a block boundary).
    trustedNumeric(f64op: string, l: Inst, r: Inst): Inst {
        if (this.mod_ctx.typed_stats)
            this.mod_ctx.typed_stats.trusted = (this.mod_ctx.typed_stats.trusted ?? 0) + 1;
        const ua = this.b.emit("unbox_f64", [l], {});
        const ub = this.b.emit("unbox_f64", [r], {});
        const v = this.b.emit(f64op, [ua, ub], {});
        if (f64op !== "f64_lt") return this.b.emit("box_f64", [v], {});
        const t_bb = this.b.newBlock("trust_lt_true");
        const f_bb = this.b.newBlock("trust_lt_false");
        const join_bb = this.b.newBlock("trust_lt_join");
        const result = join_bb.addParam("lt");
        this.b.condBr(v, t_bb, [], f_bb, []);
        this.b.sealBlock(t_bb);
        this.b.sealBlock(f_bb);
        this.b.setInsertPoint(t_bb);
        this.b.br(join_bb, [this.b.constBool(true)]);
        this.b.setInsertPoint(f_bb);
        this.b.br(join_bb, [this.b.constBool(false)]);
        this.b.sealBlock(join_bb);
        this.b.setInsertPoint(join_bb);
        return result;
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

    // Could this operand be a number at runtime?  The permissive twin of
    // operandIsNumber, for untrusted-clone bodies only: a diamond's guard
    // decides at runtime, so the only reason NOT to emit one is a proof
    // it can never pass — a non-numeric literal, or an oracle answer that
    // positively excludes number.  top/unmapped nodes assume-and-guard.
    operandPlausiblyNumber(node: e.Expression): boolean {
        if (node.type === "Literal") return typeof node.value === "number";
        if (
            node.type === "UnaryExpression" &&
            (node.operator === "-" || node.operator === "+") &&
            node.argument.type === "Literal"
        )
            return typeof (node.argument as e.Literal).value === "number";
        if (!this.oracle) return true;
        const t = this.oracle.typeOfNode(node);
        return t.tags === "top" || t.tags.has("number");
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

    // --- shape-guarded property access ---------------------
    //
    // The promotion policy (criteria 1/2 of the plan): a diamond is emitted
    // only for an EXACT receiver-shape fact — monomorphic, non-megamorphic,
    // uncapped, ordered witness present, every field's repr a single tag,
    // and the accessed field actually in the shape.  Anything less is a
    // counted decline and today's generic op.  Guarded consumption is
    // correct even when the oracle is wrong: the has_shape compare decides
    // at runtime, and a failed guard costs speed, never behavior.
    // -fno-shape-guards is the compile-time bisect hook (the
    // -fno-eir-opt mold); runtime EJS_SHAPES=off makes every guard fail.

    shapeDecline(reason: string): null {
        const stats = this.mod_ctx.typed_stats;
        if (stats) {
            const d = (stats.shape_declined ??= {});
            d[reason] = (d[reason] ?? 0) + 1;
        }
        return null;
    }

    // --types-dump: one census line per consulted access site
    shapeDumpSite(objNode: e.Expression, atom: string, what: string): void {
        if (!this.mod_ctx.shape_dump) return;
        const loc = (objNode as { loc?: { start?: { line: number; column: number } } }).loc;
        const where = loc && loc.start ? `${loc.start.line}:${loc.start.column + 1}` : "synthetic";
        console.warn(`--types-dump: shapes: .${atom} @${where}: ${what}`);
    }

    // the exact shape facts for accessing `atom` on the value of `objNode`
    // — one fact per oracle shape (two = the polymorphic chain), or
    // null (with the decline counted) when anything is short of exact.
    // Every shape in a multi-shape answer must carry the field: a shape
    // that lacks it would need the fast arm to run proto-lookup semantics,
    // which only the generic path performs (criterion 2 — no near-misses).
    // -fno-poly-shape-guards bisects polymorphic chains: 2-shape sites
    // decline "polymorphic" exactly as they did before the extension.
    // --- class-this receiver coverage ------------------------
    //
    // A base-class method's `this` receiver has the class's birth shape
    // whenever the instance came from `new C(...)` and its constructor
    // prefix took the batched fill — so `this.x` sites in methods can
    // guard on that shape with NO analysis coverage of the method body.
    // Checked tier: a foreign `this` (m.call(o)), a shape-transitioned
    // instance, or EJS_SHAPES=off just misses the guard and runs the
    // generic path.  The facts must match the fill EXACTLY, so they
    // exist only when the ctor prefix qualifies for batching under the
    // same rules (same fields, same reprs → same interned key) and no
    // later `this.<x> =` store can extend the shape past the prefix
    // (arrows inside the ctor share its `this` and are scanned too;
    // plain nested functions have their own).

    classBirthShape(
        ctorFn: e.Function | undefined,
        fieldNames: string[] | undefined
    ): ShapeField[] | null {
        const memo = (this.mod_ctx.class_shapes ??= new Map());
        const memoKey = (fieldNames ?? ctorFn) as object;
        const hit = memo.get(memoKey);
        if (hit !== undefined) return hit;
        const compute = (): ShapeField[] | null => {
            if (fieldNames) {
                // field-declaring class: the %defineField prologue makes
                // the instance shape the declared list (all boxed —
                // fields initialize undefined), and the constructor's
                // leading `this.x = v` run then repr-transitions any
                // field it stores a number into (the runtime's
                // transition_set).  So the final shape is the declared
                // ORDER with per-field reprs from the ctor stores; any
                // this-store outside that leading run (conditional,
                // effectful, or to an undeclared name) leaves the final
                // repr unknowable and declines the class.
                if (fieldNames.length < 1 || fieldNames.length > EJS_SHAPE_FIELD_CAP_MAX)
                    return null;
                const reprByName = new Map<string, "boxed" | "f64">();
                if (ctorFn) {
                    if (ctorFn.body.type !== "BlockStatement") return null;
                    const { names: storeNames, valueNodes } = this.ctorPrefixExtract(ctorFn.body);
                    if (thisStoreOutside(ctorFn.body.body.slice(storeNames.length), new Set()))
                        return null;
                    for (let i = 0; i < storeNames.length; i++) {
                        if (!fieldNames.includes(storeNames[i]!)) return null;
                        reprByName.set(
                            storeNames[i]!,
                            this.operandIsNumber(valueNodes[i]!) ? "f64" : "boxed"
                        );
                    }
                }
                return fieldNames.map((name) => ({
                    name,
                    repr: reprByName.get(name) ?? ("boxed" as const),
                }));
            }
            if (!ctorFn || ctorFn.body.type !== "BlockStatement") return null;
            const body = ctorFn.body;
            const { names, valueNodes } = this.ctorPrefixExtract(body);
            if (names.length < 2 || names.length > EJS_SHAPE_FIELD_CAP_MAX) return null;
            if (thisStoreOutside(body.body.slice(names.length), new Set(names))) return null;
            return names.map((name, i) => ({
                name,
                repr: this.operandIsNumber(valueNodes[i]!) ? ("f64" as const) : ("boxed" as const),
            }));
        };
        const fields = compute();
        memo.set(memoKey, fields);
        return fields;
    }

    classThisFacts(
        objNode: e.Expression,
        atom: string
    ): { key: string; slot: number; repr: "boxed" | "f64" }[] | null {
        if (!passes().classThisGuards) return null;
        if (objNode.type !== "ThisExpression") return null;
        if (!passes().bornShaped) return null; // the fill is what makes the shape real
        // `this` belongs to the nearest non-arrow ancestor — its node
        // carries the desugar's marker when it is a base-class method
        let fi: FnInfo | null = this.info;
        while (fi && fi.node.type === "ArrowFunctionExpression") fi = fi.parent;
        if (!fi) return null;
        const marked = fi.node as unknown as Record<string, unknown>;
        const ctorFn = marked["ejs_class_ctor_fn"] as e.Function | undefined;
        const fieldNames = marked["ejs_class_field_names"] as string[] | undefined;
        if (!ctorFn && !fieldNames) return null;
        const fields = this.classBirthShape(ctorFn, fieldNames);
        if (!fields) return null;
        const slot = fields.findIndex((f) => f.name === atom);
        if (slot < 0) return null; // method/proto access: leave the decline standing
        const key = this.module.internShape(fields);
        const stats = this.mod_ctx.typed_stats;
        if (stats) stats.shape_guards = (stats.shape_guards ?? 0) + 1;
        this.shapeDumpSite(objNode, atom, `guarded class-this shape="${key}" slot=${slot}`);
        return [{ key, slot, repr: fields[slot]!.repr }];
    }

    shapeFactFor(
        objNode: e.Expression | null,
        atom: string
    ): { key: string; slot: number; repr: "boxed" | "f64" }[] | null {
        if (!objNode || !this.oracle || !this.oracle.receiverShapeOfNode) return null;
        if (!passes().shapeGuards) return null;
        const stats = this.mod_ctx.typed_stats;
        if (stats) stats.shape_sites = (stats.shape_sites ?? 0) + 1;
        const q = this.oracle.receiverShapeOfNode(objNode);
        if (q.declined !== undefined) {
            // no analysis coverage — the class-this birth shape may
            // still answer (methods are exactly where coverage is thin)
            const ctf = this.classThisFacts(objNode, atom);
            if (ctf) return ctf;
            this.shapeDumpSite(objNode, atom, `declined ${q.declined}`);
            return this.shapeDecline(q.declined);
        }
        if (q.shapes.length > 1 && !passes().polyShapeGuards) {
            this.shapeDumpSite(objNode, atom, "declined polymorphic");
            return this.shapeDecline("polymorphic");
        }
        const facts: { key: string; slot: number; repr: "boxed" | "f64" }[] = [];
        for (const fields of q.shapes) {
            const slot = fields.findIndex((f) => f.name === atom);
            if (slot < 0) {
                this.shapeDumpSite(objNode, atom, "declined no-field");
                return this.shapeDecline("no-field"); // proto/method access
            }
            const key = this.module.internShape(fields);
            // structurally-equal shapes reported twice guard once
            if (!facts.some((f) => f.key === key))
                facts.push({ key, slot, repr: fields[slot]!.repr });
        }
        if (facts.length === 0) return this.shapeDecline("unmapped");
        if (stats) {
            stats.shape_guards = (stats.shape_guards ?? 0) + 1;
            if (facts.length > 1)
                stats.shape_poly_guards = (stats.shape_poly_guards ?? 0) + 1;
        }
        this.shapeDumpSite(
            objNode,
            atom,
            facts.map((f) => `guarded shape="${f.key}" slot=${f.slot}`).join(" | ")
        );
        return facts;
    }

    // obj.atom: a has_shape chain whose fast arms are fixed-slot loads and
    // whose shared slow arm is today's generic get — the numericDiamond
    // skeleton with one guard per exact fact.  One fact is the mono
    // diamond exactly; two facts (the polymorphic extension) test the
    // second shape on the first guard's miss edge, so each fast arm sits
    // under its own same-block-fresh has_shape fact and the verifier's
    // rules apply per arm unchanged.
    propGet(objNode: e.Expression | null, obj: Inst, atom: string): Inst {
        const facts = this.shapeFactFor(objNode, atom);
        if (!facts) return this.b.emit("get_prop_atom", [obj], { atom: atom });

        const fast_bbs = facts.map(() => this.b.newBlock("shape_fast"));
        const chk_bbs = facts.slice(1).map(() => this.b.newBlock("shape_chk"));
        const slow_bb = this.b.newBlock("shape_slow");
        const join_bb = this.b.newBlock("shape_join");
        const result = join_bb.addParam("prop");

        for (let i = 0; i < facts.length; i++) {
            const f = facts[i]!;
            const miss = i + 1 < facts.length ? chk_bbs[i]! : slow_bb;
            const t = this.b.emit("has_shape", [obj], { shape: f.key });
            this.b.condBr(t, fast_bbs[i]!, [], miss, []);
            this.b.sealBlock(fast_bbs[i]!);
            this.b.sealBlock(miss);
            if (miss !== slow_bb) this.b.setInsertPoint(miss);
        }

        for (let i = 0; i < facts.length; i++) {
            const f = facts[i]!;
            this.b.setInsertPoint(fast_bbs[i]!);
            const v = this.b.emit("slot_load", [obj], { shape: f.key, slot: f.slot, repr: f.repr });
            if (f.repr === "f64") {
                // typed slots: the load produces a raw f64 (the guard
                // proved the repr; the slot bytes ARE the double).  Box once at
                // the fast exit — the join stays boxed (its slow edge is the
                // generic get), and the optimizer's region fusion + rawJoin
                // machinery strips the box wherever the consumer is raw.
                v.type = "f64";
                const stats = this.mod_ctx.typed_stats;
                if (stats) stats.typed_loads = (stats.typed_loads ?? 0) + 1;
                const boxed = this.b.emit("box_f64", [v], {});
                this.b.br(join_bb, [boxed]);
            } else {
                this.b.br(join_bb, [v]);
            }
        }

        this.b.setInsertPoint(slow_bb);
        const g = this.b.emit("get_prop_atom", [obj], { atom: atom });
        this.b.br(join_bb, [g]);
        this.b.sealBlock(join_bb);

        this.b.setInsertPoint(join_bb);
        return result;
    }

    // obj.atom = v: the store dual.  The fast arm must prove the stored
    // value's runtime repr matches the field's shape repr (a mismatched
    // store owes a shape TRANSITION, which only the generic path performs),
    // so the guard is has_shape AND a has_tag(number) check oriented by the
    // field repr — f64 fields take numbers fast, boxed fields take
    // non-numbers fast, everything else goes generic.
    propSet(objNode: e.Expression | null, obj: Inst, atom: string, v: Inst): void {
        // 6.2.4.2 PutValue: strict-mode member stores throw on failure
        const imms = this.info.strict ? { atom: atom, strict: 1 } : { atom: atom };
        const facts = this.shapeFactFor(objNode, atom);
        if (!facts) {
            this.b.emit("set_prop_atom", [obj, v], imms);
            return;
        }

        // per-fact tag+fast pair (mono creation order preserved: tag,
        // fast, slow, join), then the chain blocks
        const tag_bbs = facts.map(() => this.b.newBlock("shape_settag"));
        const fast_bbs = facts.map(() => this.b.newBlock("shape_setfast"));
        const chk_bbs = facts.slice(1).map(() => this.b.newBlock("shape_setchk"));
        const slow_bb = this.b.newBlock("shape_setslow");
        const join_bb = this.b.newBlock("shape_setjoin");

        for (let i = 0; i < facts.length; i++) {
            const f = facts[i]!;
            const miss = i + 1 < facts.length ? chk_bbs[i]! : slow_bb;
            const t = this.b.emit("has_shape", [obj], { shape: f.key });
            this.b.condBr(t, tag_bbs[i]!, [], miss, []);
            this.b.sealBlock(tag_bbs[i]!);
            if (miss !== slow_bb) this.b.sealBlock(miss);

            this.b.setInsertPoint(tag_bbs[i]!);
            const isnum = this.b.emit("has_tag", [v], { tag: "number" });
            if (f.repr === "f64") this.b.condBr(isnum, fast_bbs[i]!, [], slow_bb, []);
            else this.b.condBr(isnum, slow_bb, [], fast_bbs[i]!, []);
            this.b.sealBlock(fast_bbs[i]!);
            // slow's predecessors: every tag block plus the last miss edge
            if (i === facts.length - 1) this.b.sealBlock(slow_bb);
            if (i + 1 < facts.length) this.b.setInsertPoint(chk_bbs[i]!);
        }

        for (let i = 0; i < facts.length; i++) {
            const f = facts[i]!;
            this.b.setInsertPoint(fast_bbs[i]!);
            if (f.repr === "f64") {
                // typed slots: unbox under the has_tag guard (the true
                // edge into this block proved v is a number, so the bits are
                // the double) and store raw — the type system carries the
                // repr proof the verifier's store rule now requires.
                const raw = this.b.emit("unbox_f64", [v], {});
                this.b.emit("slot_store", [obj, raw], { shape: f.key, slot: f.slot, repr: f.repr });
                const stats = this.mod_ctx.typed_stats;
                if (stats) stats.typed_stores = (stats.typed_stores ?? 0) + 1;
            } else {
                this.b.emit("slot_store", [obj, v], { shape: f.key, slot: f.slot, repr: f.repr });
            }
            this.b.br(join_bb, []);
        }

        this.b.setInsertPoint(slow_bb);
        this.b.emit("set_prop_atom", [obj, v], imms);
        this.b.br(join_bb, []);
        this.b.sealBlock(join_bb);

        this.b.setInsertPoint(join_bb);
    }

    // --- the fenced constructor prefix ---------------------
    //
    // Detect the maximal leading run of `this.<name> = <literal-or-local>`
    // statements in a plain function body and batch it into ONE guarded
    // fill_object_shaped diamond.  The fence is structural and oracle-free
    // (the specialization discipline — a lying oracle cannot make this wrong):
    //
    //   - plain function, not an arrow (whose `this` is lexical), not the
    //     toplevel, not a specialization clone;
    //   - every stored value is a Literal or an Identifier resolving to a
    //     local binding — evaluating it cannot run user code, so hoisting
    //     the evaluations above the batched stores is observably identical;
    //   - nothing else appears between the stores (they are consecutive
    //     statements), so no code can observe the receiver mid-prefix —
    //     `"y" in this` between stores, an escaping call, a getter-running
    //     value all CUT the prefix at that statement;
    //   - names distinct, non-index-looking, not __proto__, count within
    //     the runtime's shaped field cap.
    //
    // The batching is additionally guarded at runtime by has_shape(this, "")
    // — only a construct-fresh EMPTY receiver takes the fast arm; a reused
    // this (F.call(o)), a dictionary-mode object, or EJS_SHAPES=off all
    // fail the one-compare guard and run the original sequential stores.
    // The runtime call re-checks everything again (incl. proto-chain
    // accessor interception) and falls back to sequential [[Set]]s, so a
    // wrong guard can cost speed, never behavior.  -fno-born-shaped is
    // the bisect hook.  Returns how many leading statements were consumed.

    fenceDecline(reason: string): void {
        const stats = this.mod_ctx.typed_stats;
        if (stats) {
            const d = (stats.fence_declined ??= {});
            d[reason] = (d[reason] ?? 0) + 1;
        }
    }

    // the maximal leading `this.<name> = <literal-or-local>` run of a
    // ctor-shaped body — shared by the ctor-prefix batching below and
    // the class-this shape facts (classBirthShape), which must agree
    // exactly on the fields for the guard key to match the fill
    ctorPrefixExtract(body: e.BlockStatement): {
        names: string[];
        valueNodes: e.Expression[];
        cutReason: string | null;
        consumed: number;
    } {
        const names: string[] = [];
        const valueNodes: e.Expression[] = [];
        let cutReason: string | null = null;
        for (const s of body.body) {
            const cut = (why: string): true => ((cutReason = why), true);
            if (s.type !== "ExpressionStatement") break;
            const a = s.expression;
            if (a.type !== "AssignmentExpression" || a.operator !== "=") break;
            const m = a.left;
            if (m.type !== "MemberExpression" || m.computed) break;
            if (m.object.type !== "ThisExpression") break;
            if (m.property.type !== "Identifier") break;
            const name = m.property.name;
            if (name === "__proto__" || /^[0-9]/.test(name)) {
                cut("unshapeable-name");
                break;
            }
            if (names.includes(name)) {
                cut("duplicate-name");
                break;
            }
            const v = a.right;
            if (v.type !== "Literal" && !(v.type === "Identifier" && this.analysis.resolve(v))) {
                cut("value-not-local");
                break;
            }
            names.push(name);
            valueNodes.push(v);
        }
        return { names, valueNodes, cutReason, consumed: names.length };
    }

    lowerBornShapedCtorPrefix(body: e.BlockStatement): number {
        if (!this.oracle || !passes().bornShaped) return 0;
        if (this.isToplevel || this.spec) return 0;
        if (this.info.node.type === "ArrowFunctionExpression") return 0;

        const { names, valueNodes, cutReason } = this.ctorPrefixExtract(body);
        if (names.length < 2) {
            // a ctor-looking body (at least one conforming this-store) that
            // did not reach the batching threshold is a counted decline;
            // everything else simply is not a constructor prefix
            if (names.length === 1) this.fenceDecline(cutReason ?? "short-prefix");
            return 0;
        }
        if (names.length > EJS_SHAPE_FIELD_CAP_MAX) {
            this.fenceDecline("capped");
            return 0;
        }

        // values first (locals/literals — effect-free), then the guard
        const values = valueNodes.map((v) => this.expr(v));
        const fields: ShapeField[] = names.map((name, i) => ({
            name,
            repr: this.operandIsNumber(valueNodes[i]!) ? ("f64" as const) : ("boxed" as const),
        }));
        const key = this.module.internShape(fields);
        this.module.internShape([]); // the guard's empty shape
        const thisVal = this.b.readVariable("%this", this.b.cur);

        const fast_bb = this.b.newBlock("ctor_fill_fast");
        const slow_bb = this.b.newBlock("ctor_fill_slow");
        const join_bb = this.b.newBlock("ctor_fill_join");
        const t = this.b.emit("has_shape", [thisVal], { shape: "" });
        this.b.condBr(t, fast_bb, [], slow_bb, []);
        this.b.sealBlock(fast_bb);
        this.b.sealBlock(slow_bb);

        this.b.setInsertPoint(fast_bb);
        this.b.emit("fill_object_shaped", [thisVal, ...values], { shape: key });
        this.b.br(join_bb, []);

        this.b.setInsertPoint(slow_bb);
        for (let i = 0; i < names.length; i++)
            this.b.emit("set_prop_atom", [thisVal, values[i]!], { atom: names[i]! });
        this.b.br(join_bb, []);
        this.b.sealBlock(join_bb);
        this.b.setInsertPoint(join_bb);

        const stats = this.mod_ctx.typed_stats;
        if (stats) stats.ctor_fills = (stats.ctor_fills ?? 0) + 1;
        return names.length;
    }

    logical(n: e.LogicalExpression): Inst {
        let l = this.expr(n.left);

        let rhs_bb = this.b.newBlock("logical_rhs");
        let join_bb = this.b.newBlock("logical_join");
        let result = join_bb.addParam("logical");

        if (n.operator === "??") {
            // nullish: evaluate the rhs only when the lhs is null or
            // undefined — exactly what `== null` tests (no valueOf hooks)
            let isnullish = this.b.emit("loose_eq", [l, this.b.constNull()], {});
            let lbool = this.b.emit("to_boolean", [isnullish], {});
            this.b.condBr(lbool, rhs_bb, [], join_bb, [l]);
        } else {
            let lbool = this.b.emit("to_boolean", [l], {});
            if (n.operator === "&&") this.b.condBr(lbool, rhs_bb, [], join_bb, [l]);
            else if (n.operator === "||") this.b.condBr(lbool, join_bb, [l], rhs_bb, []);
            else throw LowerNotSupported(`logical operator ${n.operator}`, n.loc);
        }
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
            case "typeof": {
                // typeof of an unresolvable name is "undefined", never a
                // ReferenceError — mark the global load so it skips the
                // checked (throwing) read
                if (n.argument.type === "Identifier") {
                    const idn = n.argument as e.Identifier;
                    if (
                        idn.name !== "undefined" &&
                        !this.analysis.resolve(idn) &&
                        !this.mod_ctx.refs.get(idn.name)
                    ) {
                        arg = this.b.emit("get_global", [], { atom: idn.name, for_typeof: 1 });
                        return this.b.emit("typeof", [arg], {});
                    }
                }
                arg = this.expr(n.argument);
                return this.b.emit("typeof", [arg], {});
            }
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
                        : this.expr(m.property as e.Expression);
                return this.b.emit(
                    "delete_prop",
                    [obj, key],
                    this.info.strict ? { strict: 1 } : {}
                );
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
            this.b.emit(
                "set_global",
                [value],
                this.info.strict ? { atom: idNode.name, strict: 1 } : { atom: idNode.name }
            );
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
            const objNode = n.left.object as e.Expression;
            const obj = this.expr(objNode);
            let atom: string | null = null;
            let key: Inst | null = null;
            if (!n.left.computed && n.left.property.type === "Identifier")
                atom = n.left.property.name;
            else key = this.expr(n.left.property as e.Expression);
            let v: Inst;
            if (binop) {
                const cur =
                    atom !== null
                        ? this.propGet(objNode, obj, atom)
                        : this.b.emit("get_prop", [obj, key!], {});
                const rhs = this.expr(n.right);
                v = this.b.emit(binop, [cur, rhs], {});
            } else {
                v = this.expr(n.right);
            }
            if (atom !== null) this.propSet(objNode, obj, atom, v);
            else
                this.b.emit(
                    "set_prop",
                    [obj, key!, v],
                    this.info.strict ? { strict: 1 } : {}
                );
            return v;
        }
        throw LowerNotSupported(`assignment target ${n.left.type}`, n.loc);
    }

    // ++/--: ToNumeric(old value), then add/sub 1 (the `update` imm
    // keeps BigInt increments off the mixed-operand TypeError)
    update(n: e.UpdateExpression): Inst {
        let one = this.b.constNumber(1);
        let op = n.operator === "++" ? "add" : "sub";
        if (n.argument.type === "Identifier") {
            let cur = this.identifier(n.argument);
            let old = this.b.emit("to_numeric", [cur], {});
            let nv = this.b.emit(op, [old, one], { update: 1 });
            this.writeIdentifier(n.argument, nv);
            return n.prefix ? nv : old;
        }
        if (n.argument.type === "MemberExpression") {
            const m = n.argument;
            const objNode = m.object as e.Expression;
            const obj = this.expr(objNode);
            let atom: string | null = null;
            let key: Inst | null = null;
            if (!m.computed && m.property.type === "Identifier") atom = m.property.name;
            else key = this.expr(m.property as e.Expression);
            const cur =
                atom !== null
                    ? this.propGet(objNode, obj, atom)
                    : this.b.emit("get_prop", [obj, key!], {});
            const old = this.b.emit("to_numeric", [cur], {});
            const nv = this.b.emit(op, [old, one], { update: 1 });
            if (atom !== null) this.propSet(objNode, obj, atom, nv);
            else
                this.b.emit(
                    "set_prop",
                    [obj, key!, nv],
                    this.info.strict ? { strict: 1 } : {}
                );
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
                let key = this.expr(n.tag.property as e.Expression);
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
        // regex literals never fold here even though a rejected one has
        // value === null: make_regexp builds a FRESH object per
        // evaluation, and folding would mint one per reference site
        // where the export must be a single identity
        if (cv && cv.type === "Literal" && !cv.regex && (cv.value === null || typeof cv.value !== "object"))
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
            return this.propGet(n.object as e.Expression, obj, n.property.name);
        let key = this.expr(n.property as e.Expression);
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
                callee = this.propGet(
                    n.callee.object as e.Expression,
                    thisArg,
                    n.callee.property.name
                );
            else {
                let key = this.expr(n.callee.property as e.Expression);
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
        // the generator body forms lower specially: yields are
        // gen_yield ops (suspension points gen-lower.ts rewrites), and
        // yield* is an inline delegation loop — a state machine can only
        // suspend its own frame, so the legacy nested-helper yield is
        // structurally unavailable
        if (calleeName === "%generatorYield") {
            if (!this.isGenBody)
                throw LowerNotSupported("%generatorYield outside a generator body", n.loc);
            const g = this.expr(n.arguments[0] as e.Expression);
            const v = this.expr(n.arguments[1] as e.Expression);
            return this.b.emit("gen_yield", [g, v], {});
        }
        if (calleeName === "%generatorDelegate") {
            if (!this.isGenBody)
                throw LowerNotSupported("%generatorDelegate outside a generator body", n.loc);
            return this.lowerGeneratorDelegate(
                n.arguments[0] as e.Expression,
                n.arguments[1] as e.Expression
            );
        }
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
                        len: specFnLength(childInfo.node),
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
            case "ExportAllDeclaration": {
                if (!this.isToplevel) throw LowerNotSupported("export declaration", n.loc);
                const source = n.source_path!.value;
                const source_info =
                    this.mod_ctx.module_infos && this.mod_ctx.module_infos.get(source);
                if (!source_info || source_info.isNative())
                    throw LowerNotSupported(`re-export from '${source}'`, n.loc);
                if (n.exported) {
                    // export * as ns from "m": bind the source module's
                    // namespace object to our `ns` slot.  member reads on
                    // it resolve at runtime through the module object's
                    // export accessors.
                    const ns = this.b.emit("module_get_exotic", [], { module: source });
                    this.storeExportSlot(n.exported.name, ns, n.loc);
                    return;
                }
                // export * from "m": copy the source module's slots into
                // the same-named slots of ours at init time (a snapshot,
                // exactly like `export { a } from "m"`).  The name list
                // was computed by gather-imports' star expansion.
                for (const name of n.star_export_names ?? []) {
                    const export_info = source_info.exports.get(name);
                    if (!export_info || export_info.promoted) continue;
                    const v = this.b.emit("module_slot_load", [], {
                        module: source,
                        slot: export_info.slot_num,
                    });
                    this.storeExportSlot(name, v, n.loc);
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
                // trusted clone with an f64 result: return the raw f64
                // (unguarded unbox — the same trust as trustedNumeric).
                // A return this can't prove leaves a boxed return that the
                // structural post-check in specialize.ts rejects, so a
                // clone never ships with a sig its returns don't honor.
                // (untrusted clones always carry a boxed "any" result.)
                if (this.spec && this.spec.trusted && this.spec.result === "f64" &&
                    n.argument && this.operandIsNumber(n.argument))
                    rv = this.b.emit("unbox_f64", [rv], {});
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
            if (prop.type === "RestElement")
                throw LowerNotSupported("rest property in declaration pattern", d.loc);
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
            const v = this.propGet(d.init ?? null, src, keyName);
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

    // yield*: the delegation loop inlines into the body, because
    // gen_yield can only suspend THIS function's frame.  Sent values
    // forward into the inner iterator's next(); an abrupt resume at the
    // suspended yield (gen.throw()/gen.return(), the return sentinel
    // included) closes the inner iterator and rethrows; the loop's
    // value is the inner return value.  One static next() site means
    // the first call passes undefined rather than no argument —
    // indistinguishable to any iterator treating absent as undefined.
    lowerGeneratorDelegate(genArg: e.Expression, iterableArg: e.Expression): Inst {
        const gen = this.expr(genArg);
        const obj = this.expr(iterableArg);
        const sym = this.b.emit("get_global", [], { atom: "Symbol" });
        const itkey = this.b.emit("get_prop_atom", [sym], { atom: "iterator" });
        const itfn = this.b.emit("get_prop", [obj, itkey], {});
        const iter = this.b.emit("call", [itfn, obj], {});

        const sentVar = `%gendel#${this.b.fn.newValueId()}`;
        this.b.writeVariable(sentVar, this.b.cur, this.b.constUndefined());

        const header = this.b.newBlock("gendel_header");
        const body = this.b.newBlock("gendel_body");
        const close = this.b.newCatchBlock("gendel_close");
        const exit = this.b.newBlock("gendel_exit");

        this.b.br(header, []);
        this.b.setInsertPoint(header);
        const nextfn = this.b.emit("get_prop_atom", [iter], { atom: "next" });
        const res = this.b.emit(
            "call",
            [nextfn, iter, this.b.readVariable(sentVar, this.b.cur)],
            {}
        );
        const done = this.b.emit("get_prop_atom", [res], { atom: "done" });
        const dbool = this.b.emit("to_boolean", [done], {});
        this.b.condBr(dbool, exit, [], body, []);
        this.b.sealBlock(body);

        this.b.setInsertPoint(body);
        const v = this.b.emit("get_prop_atom", [res], { atom: "value" });
        this.b.pushHandler(close);
        const sent = this.b.emit("gen_yield", [gen, v], {});
        this.b.popHandler();
        this.b.writeVariable(sentVar, this.b.cur, sent);
        this.b.br(header, []);
        this.b.sealBlock(header);
        this.b.sealBlock(close);

        // IteratorClose: `if (it.return != null) it.return(); throw exc`
        // (an exception from it.return() replaces the original, like the
        // helper's catch body)
        this.b.setInsertPoint(close);
        const exc = close.params[0]!;
        const retfn = this.b.emit("get_prop_atom", [iter], { atom: "return" });
        const neq = this.b.emit("loose_neq", [retfn, this.b.constNull()], {});
        const nb = this.b.emit("to_boolean", [neq], {});
        const do_close = this.b.newBlock("gendel_do_close");
        const rethrow = this.b.newBlock("gendel_rethrow");
        this.b.condBr(nb, do_close, [], rethrow, []);
        this.b.sealBlock(do_close);
        this.b.setInsertPoint(do_close);
        this.b.emit("call", [retfn, iter], {});
        if (!this.b.cur.terminated) this.b.br(rethrow, []);
        this.b.sealBlock(rethrow);
        this.b.setInsertPoint(rethrow);
        this.b.throwValue(exc);

        this.b.sealBlock(exit);
        this.b.setInsertPoint(exit);
        return this.b.emit("get_prop_atom", [res], { atom: "value" });
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

        // When every case test is a string literal, one table probe
        // replaces the whole strict_eq chain: atom_switch_index returns
        // the first matching case's table position (boxed; -1 for a
        // non-string or unmatched discriminant) and each case tests its
        // own position with a machine compare.  Chain semantics hold
        // exactly — positions follow document order, duplicate strings
        // resolve to the first occurrence inside the probe, and string
        // literals have no evaluation effects to skip.
        const stringCases: number[] = [];
        for (let i = 0; i < n.cases.length; i++) {
            const test = n.cases[i]!.test;
            if (!test) continue;
            if (test.type !== "Literal" || typeof (test as e.Literal).value !== "string") {
                stringCases.length = 0;
                break;
            }
            stringCases.push(i);
        }
        if (passes().atomSwitch && stringCases.length >= 2) {
            const atoms = stringCases.map((i) => String((n.cases[i]!.test as e.Literal).value));
            const idx = this.b.emit("atom_switch_index", [disc], { atoms });
            for (let k = 0; k < stringCases.length; k++) {
                const i = stringCases[k]!;
                const cmp = this.b.emit("switch_index_eq", [idx], { index: k });
                const next_test = this.b.newBlock(`case_test${i}`);
                this.b.condBr(cmp, bodies[i]!, [], next_test, []);
                this.b.sealBlock(next_test);
                this.b.setInsertPoint(next_test);
            }
        } else {
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
    if (info.node.body.type === "BlockStatement") {
        // a fenced constructor's leading this-store run
        // batches into one guarded fill; the remaining statements lower
        // exactly as the BlockStatement case would have
        const skip = lf.lowerBornShapedCtorPrefix(info.node.body);
        for (let i = skip; i < info.node.body.body.length; i++) {
            lf.stmt(info.node.body.body[i]!);
            if (lf.b.cur.terminated) break;
        }
    } else lf.b.ret(lf.expr(info.node.body)); // expression-bodied arrow
    info.fn = lf.finish();
    module.addFunction(info.fn);
    // hoisted closures may reference children whose declaration statement
    // was never reached (e.g. behind an early return); every child still
    // needs a body in the module.
    for (let child of info.children) lowerOneFunction(child, analysis, module, mod_ctx);
    return info.fn;
}

// lower a specialized clone of an already-lowered function.
// Unlike lowerOneFunction this ignores info.lowered/info.fn (the generic
// lowering stands), gives the Func the clone's name and typed sig, and
// lowers oracle-number arithmetic unguarded (SpecMode).  Children were
// lowered with the generic pass and are shared by name (make_closure in
// the clone body resolves to the same child Funcs).  The caller
// (specialize.ts) owns the structural post-checks and adds the Func to
// the module only when they pass.
export function lowerSpecializedClone(
    info: FnInfo,
    analysis: ScopeAnalysis,
    module: Module,
    mod_ctx: ModCtx,
    spec: SpecMode
): Func {
    const lf = new LowerFunction(info, analysis, module, mod_ctx, spec);
    if (info.node.body.type === "BlockStatement") lf.stmt(info.node.body);
    else {
        // expression-bodied arrow: same typed-return rule as ReturnStatement
        let rv = lf.expr(info.node.body);
        if (spec.trusted && spec.result === "f64" && lf.operandIsNumber(info.node.body as e.Expression))
            rv = lf.b.emit("unbox_f64", [rv], {});
        lf.b.ret(rv);
    }
    return lf.finish();
}

// lower a FunctionDeclaration/FunctionExpression AST node into a fresh
// module; returns { module, fn }
export function lowerFunctionNode(
    n: e.Function,
    name?: string,
    oracle?: TypeOracle | null
): { module: Module; fn: Func; diamonds: number; shape_guards: number } {
    let analysis = new ScopeAnalysis();
    let info = analysis.analyzeFunction(n, name);
    let module = new Module(info.name);
    let typed_stats: NonNullable<ModCtx["typed_stats"]> = { diamonds: 0 };
    let fn = lowerOneFunction(info, analysis, module, {
        refs: new Map(),
        oracle: oracle ?? null,
        typed_stats,
    });
    return {
        module: module,
        fn: fn,
        diamonds: typed_stats.diamonds,
        shape_guards: typed_stats.shape_guards ?? 0,
    };
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
