/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// Scope analysis for EIR lowering:
//
//   - every binding (param, var/let/const, function decl, catch param) gets
//     a unique id, so shadowing never aliases SSA variables;
//   - every Identifier reference is resolved to its binding (or to null,
//     meaning "global") in a side map keyed by AST node;
//   - a binding referenced from a function nested below its declaration is
//     marked captured and assigned an environment slot in the declaring
//     function; functions learn their env size and whether their env must
//     carry a parent-env pointer in slot 0.
//
// The walker deliberately covers the same whitelisted AST subset as
// lower.ts and throws LowerNotSupported on anything else, early — a
// construct that doesn't lower is a compile error, and it must surface
// before lowering starts mutating the module.

import type * as e from "../estree";
import { LowerNotSupported } from "./errors";
import { eir_intrinsics } from "./intrinsics";
import type { BinaryOperator } from "../estree";

// compound assignment operator -> the binary operator it desugars to
// (kept in sync with lower.ts's binops table)
export const compound_assign_ops = {
    "+=": "+",
    "-=": "-",
    "*=": "*",
    "/=": "/",
    "%=": "%",
    "**=": "**",
    "&=": "&",
    "|=": "|",
    "^=": "^",
    "<<=": "<<",
    ">>=": ">>",
    ">>>=": ">>>",
} satisfies Record<string, BinaryOperator> as Record<string, BinaryOperator | undefined>;

export type BindingKind = "param" | "local" | "fn" | "catch" | "self" | "this";

let binding_id_gen = 0;

export class Binding {
    name: string;
    uid: string;
    kind: BindingKind;
    fnInfo: FnInfo | null; // declaring FnInfo
    captured = false;
    slot = -1; // env slot, if captured
    loopEnv: LoopEnv | null = null; // LoopEnv candidate, for let/const loop bindings

    constructor(name: string, kind: BindingKind, fnInfo: FnInfo | null) {
        this.name = name;
        this.uid = `${name}#${binding_id_gen++}`;
        this.kind = kind;
        this.fnInfo = fnInfo;
    }
}

export class FnInfo {
    // discriminant against LoopEnv in env-descriptor chains (lower.ts)
    readonly isLoopEnv = false as const;
    // set by lowering (lower.ts lowerOneFunction)
    lowered = false;
    fn: import("./ir").Func | null = null;
    node: e.Function;
    name: string;
    parent: FnInfo | null;
    children: FnInfo[] = [];
    params: Binding[] = [];
    bindings: Binding[] = []; // every Binding declared here
    needsParentEnv = false; // some descendant reaches past this fn
    envSize = 0; // slots (incl. parent slot), 0 = no env
    parentSlot = -1; // slot holding the parent env, or -1
    creationLoopEnv: LoopEnv | null = null; // innermost LoopEnv at the definition site
    // set lazily by the walker
    restBinding: Binding | null = null;
    defaults: (e.Expression | null)[] = [];
    argumentsBinding: Binding | null = null;
    usesArguments = false;
    thisBinding: Binding | null = null;
    // some ThisExpression resolves to this function (directly or through
    // arrows) — drives the sloppy-mode this coercion at entry
    usesThis = false;
    isToplevel = false;
    // strict-mode code: inherited from the enclosing function or declared
    // by a "use strict" directive prologue in this body
    strict = false;

    constructor(node: e.Function, name: string, parent: FnInfo | null) {
        this.node = node;
        this.name = name;
        this.parent = parent;
        if (parent) parent.children.push(this);
        this.strict =
            (parent ? parent.strict : false) ||
            (node as unknown as Record<string, unknown>)["ejs_strict"] === true ||
            FnInfo.hasUseStrict(node);
    }

    private static hasUseStrict(node: e.Function): boolean {
        const body = (node as unknown as Record<string, unknown>)["body"] as e.Node | undefined;
        if (!body || body.type !== "BlockStatement") return false;
        for (const stmt of (body as e.BlockStatement).body) {
            // scope analysis runs post-desugar: HoistFuncDecls moves
            // function declarations ABOVE the directive prologue — skip
            // them
            if (stmt.type === "FunctionDeclaration") continue;
            if (stmt.type !== "ExpressionStatement") break;
            const expr = (stmt as e.ExpressionStatement).expression;
            if (expr.type !== "Literal" || typeof (expr as e.Literal).value !== "string") break;
            if ((expr as e.Literal).value === "use strict") return true;
        }
        return false;
    }
}

// a per-iteration environment for a loop whose let/const bindings are
// captured by closures (`for (let i ...) { use(() => i); }`): each
// iteration allocates a fresh env so every closure sees that iteration\'s
// binding.  slot 0 always holds the enclosing environment (the value of
// curEnv at loop entry).  candidates are created for every let/const
// loop declaration during the walk and materialize after it, once
// capture flags are known; unmaterialized candidates are transparent.
let loopenv_id_gen = 0;

export class LoopEnv {
    id: number;
    readonly isLoopEnv = true as const;
    fnInfo: FnInfo | null; // the function containing the loop
    node: e.Node; // the loop AST node
    parentCandidate: LoopEnv | null; // enclosing LoopEnv in the same fn, or null
    allBindings: Binding[] = []; // every let/const binding the loop declares
    bindings: Binding[] = []; // the captured subset (set at materialization)
    materialized = false;
    envSize = 0;
    parentSlot = -1; // always 0 once materialized

    constructor(fnInfo: FnInfo | null, node: e.Node, parentCandidate: LoopEnv | null) {
        this.id = loopenv_id_gen++;
        this.fnInfo = fnInfo;
        this.node = node;
        this.parentCandidate = parentCandidate;
    }
}

class LexScope {
    parent: LexScope | null;
    fnInfo: FnInfo | null;
    names = new Map<string, Binding>();
    isFnTop = false;

    constructor(parent: LexScope | null, fnInfo: FnInfo | null) {
        this.parent = parent;
        this.fnInfo = fnInfo;
    }

    declare(name: string, kind: BindingKind): Binding {
        // redeclaration in the same lexical scope reuses the binding (var x;
        // var x; — and function-level var hoisting lands them in one scope)
        const existing = this.names.get(name);
        if (existing) return existing;
        const binding = new Binding(name, kind, this.fnInfo);
        this.names.set(name, binding);
        this.fnInfo!.bindings.push(binding);
        return binding;
    }

    lookup(name: string): Binding | null {
        let s: LexScope | null = this;
        while (s) {
            const found = s.names.get(name);
            if (found) return found;
            s = s.parent;
        }
        return null;
    }
}

export interface LabelEntry {
    name: string;
    isLoop: boolean;
}

export class ScopeAnalysis {
    refs = new Map<e.Node, Binding | null>(); // Identifier node -> Binding | null (global)
    fnInfos = new Map<e.Function, FnInfo>();
    globalNames = new Set<string>(); // free names that resolved to nothing
    globalValueNames = new Set<string>(); // free names used other than as a direct callee
    globalAssignedNames = new Set<string>(); // free names that are assigned to
    anon_gen = 0;
    curScope: LexScope | null = null;
    curFn: FnInfo | null = null;
    // per-iteration loop env candidates: every let/const loop
    // declaration gets one; those with captured bindings materialize
    // after the walk (see analyzeFunction) and lowering builds a
    // fresh env per iteration.
    loopEnvs: LoopEnv[] = [];
    loopEnvStack: LoopEnv[] = []; // active candidates (innermost last)
    loopEnvByNode = new Map<e.Node, LoopEnv>(); // loop AST node -> head LoopEnv
    bodyEnvByNode = new Map<e.Node, LoopEnv>(); // loop AST node -> body LoopEnv
    // set around a for-init declaration walk so the declared bindings
    // attach to the loop's env candidate
    pendingLoopEnv: LoopEnv | null = null;
    // labels are per-function (a labeled break can't cross a function
    // boundary); enterFunction/leaveFunction save and restore
    labelStack: LabelEntry[] = [];
    savedLabelStacks: LabelEntry[][] = [];
    // toplevel-as-EIR mode (analyzeToplevel): module-scope names backed
    // by module slots (or const-literal folds).  declarations of these
    // at the root function's top level create NO local binding — every
    // reference resolves as free and the integration's refs machinery
    // routes it through the slot.
    moduleSlotNames: Set<string> | null = null;
    rootInfo: FnInfo | null = null;
    // every EIR function name handed out by enterFunction (scope
    // qualification alone isn't unique)
    usedFnNames = new Set<string>();

    // the loop's materialized head env, or null (for lowering)
    loopEnvOf(node: e.Node): LoopEnv | null {
        let le = this.loopEnvByNode.get(node);
        return le && le.materialized ? le : null;
    }

    // the loop's materialized body env, or null (for lowering)
    loopBodyEnvOf(node: e.Node): LoopEnv | null {
        let le = this.bodyEnvByNode.get(node);
        return le && le.materialized ? le : null;
    }

    resolve(node: e.Node): Binding | null | undefined {
        return this.refs.get(node);
    }

    infoFor(fnNode: e.Function): FnInfo | undefined {
        return this.fnInfos.get(fnNode);
    }

    // --- entry point ---------------------------------------------------------

    // walk a function's body without pushing a block scope: the body's
    // top-level declarations belong to the function scope itself (isFnTop),
    // otherwise every body-level function declaration would look like a
    // block-level one.
    walkFnBody(body: e.BlockStatement): void {
        // hoisting, pass 1: function-scope declarations are visible from
        // the top of the function regardless of statement order (function
        // declarations hoist, and echojs's no-TDZ let/const read as
        // undefined before their statement).  without this, a nested
        // function placed ABOVE a let/const it captures — which the
        // pre-EIR HoistFuncDecls pass produces routinely — resolved the
        // name as a global.
        for (const s of body.body) {
            let stmt: e.Statement = s;
            if (stmt.type === "ExportNamedDeclaration" && stmt.declaration)
                stmt = stmt.declaration;
            if (stmt.type === "VariableDeclaration") {
                for (let d of stmt.declarations) {
                    // patterns are pre-desugared (DesugarDestructuring runs
                    // before HoistFuncDecls); if one reaches us anyway,
                    // fall back rather than silently skip its targets —
                    // they'd misresolve as globals from any hoisted
                    // function above the declaration
                    if (d.id.type !== "Identifier")
                        throw LowerNotSupported(
                            `fn-top declaration pattern ${d.id.type}`,
                            stmt.loc
                        );
                    if (this.slotBackedDecl(d.id.name, this.curScope!)) continue;
                    this.curScope!.declare(d.id.name, "local");
                }
            } else if (stmt.type === "FunctionDeclaration" && stmt.id) {
                if (this.slotBackedDecl(stmt.id.name, this.curScope!)) continue;
                this.curScope!.declare(stmt.id.name, "fn");
            } else {
                // `var`s nested in other statements (`if (c) var x = ...`)
                // hoist to the function scope too
                this.prescanNestedVars(stmt);
            }
        }
        for (let s of body.body) this.walkStmt(s);
    }

    // pre-declare var-kind declarations at any statement depth (stopping
    // at nested functions, whose vars are their own)
    prescanNestedVars(n: unknown): void {
        if (!n || typeof n !== "object") return;
        if (Array.isArray(n)) {
            for (const el of n) this.prescanNestedVars(el);
            return;
        }
        const node = n as e.Node;
        switch (node.type) {
            case "FunctionDeclaration":
            case "FunctionExpression":
            case "ArrowFunctionExpression":
                return; // function boundary
            case "VariableDeclaration":
                if (node.kind !== "var") return; // let/const are block-scoped
                for (const d of node.declarations) {
                    if (d.id.type !== "Identifier")
                        throw LowerNotSupported(
                            `nested var declaration pattern ${d.id.type}`,
                            node.loc
                        );
                    if (this.slotBackedDecl(d.id.name, this.curScope!)) continue;
                    this.curScope!.declare(d.id.name, "local");
                }
                return;
            default:
                for (const k of Object.keys(node)) {
                    if (k === "loc") continue;
                    this.prescanNestedVars((node as unknown as Record<string, unknown>)[k]);
                }
                return;
        }
    }

    analyzeFunction(fnNode: e.Function, name?: string): FnInfo {
        // bind the function's own name outside its scope (like a named
        // function expression) so recursion resolves to a "self" binding
        // instead of looking like a global; lowering turns calls through
        // it into direct calls.
        // for an anonymous function-expression candidate (var f =
        // function () {}), the module-scope name serves as the self name:
        // integration only makes such a candidate viable when the name is
        // never reassigned.
        let selfName = (fnNode.id && fnNode.id.name) || name;
        let selfBinding = null;
        if (selfName) {
            this.curScope = new LexScope(this.curScope, this.curFn);
            selfBinding = new Binding(selfName, "self", null);
            this.curScope.names.set(selfName, selfBinding);
        }
        let info = this.enterFunction(fnNode, name);
        if (selfBinding) selfBinding.fnInfo = info;
        if (fnNode.body.type === "BlockStatement") this.walkFnBody(fnNode.body);
        else this.walkExpr(fnNode.body); // expression-bodied arrow
        this.leaveFunction();
        if (selfBinding) this.curScope = this.curScope!.parent;
        this.finishAnalysis(info);
        return info;
    }

    // toplevel-as-EIR: analyze the whole module toplevel function.  module
    // bindings named in moduleSlotNames get no local binding (their
    // declarations lower as slot stores, their references as slot loads);
    // everything else is an ordinary toplevel local.
    analyzeToplevel(
        fnNode: e.FunctionDeclaration,
        name: string,
        moduleSlotNames: Set<string>,
        // module goal: the toplevel is unconditionally strict, and every
        // nested function inherits it (set before the body walk so child
        // FnInfos see it)
        strict = false
    ): FnInfo {
        this.moduleSlotNames = moduleSlotNames;
        let info = this.enterFunction(fnNode, name);
        info.isToplevel = true;
        if (strict) info.strict = true;
        this.rootInfo = info;
        this.walkFnBody(fnNode.body);
        this.leaveFunction();
        this.finishAnalysis(info);
        return info;
    }

    finishAnalysis(info: FnInfo): void {
        // materialize the loop envs whose bindings are captured; their
        // bindings get loop-env slots (from 1; slot 0 is the parent env)
        // and are excluded from function-env slot assignment below.
        for (let le of this.loopEnvs) {
            le.bindings = le.allBindings.filter((bd) => bd.captured);
            if (le.bindings.length === 0) continue;
            le.materialized = true;
            le.parentSlot = 0;
            let next = 1;
            for (let bd of le.bindings) bd.slot = next++;
            le.envSize = next;
        }
        assignSlots(info);
    }

    // is a declaration of `name`, landing in `scope`, backed by a module
    // slot (or const-literal fold) instead of a local binding?
    slotBackedDecl(name: string, scope: LexScope): boolean {
        return (
            this.moduleSlotNames !== null &&
            this.curFn === this.rootInfo &&
            scope.isFnTop &&
            this.moduleSlotNames.has(name)
        );
    }

    enterFunction(fnNode: e.Function, name?: string): FnInfo {
        let fname = name || (fnNode.id && fnNode.id.name) || "anon";
        // scope-qualified names aren't unique on their own (an object
        // method `replace` and a toplevel function `replace` both qualify
        // to `<parent>.replace`); every EIR function in a module needs a
        // distinct symbol
        if (this.usedFnNames.has(fname)) {
            let i = 2;
            while (this.usedFnNames.has(fname + "~" + i)) i++;
            fname = fname + "~" + i;
        }
        this.usedFnNames.add(fname);
        let info = new FnInfo(fnNode, fname, this.curFn);
        this.fnInfos.set(fnNode, info);

        // the innermost loop env active at this definition site (in the
        // DEFINING function): the closure's incoming env is that loop's
        // per-iteration env, so env-chain walks must start there
        let leTop = this.loopEnvStack[this.loopEnvStack.length - 1];
        info.creationLoopEnv = leTop && leTop.fnInfo === this.curFn ? leTop : null;

        if (fnNode.generator)
            throw LowerNotSupported("generator function", fnNode.loc);

        this.savedLabelStacks.push(this.labelStack);
        this.labelStack = [];
        this.curFn = info;
        this.curScope = new LexScope(this.curScope, info);
        this.curScope.isFnTop = true;
        // the rest parameter (a trailing RestElement, or fnNode.rest in
        // older ASTs) is an ordinary local initialized from the trailing
        // arguments in the prologue (see lower.js / rest_args)
        let restId: e.Pattern | null = fnNode.rest ?? null;
        let plainParams = fnNode.params;
        let last = plainParams[plainParams.length - 1];
        if (last && last.type === "RestElement") {
            restId = last.argument;
            // (positive end index: the self-hosted runtime's slice-dense
            // fast path crashes on negative indices — see runtime bug note
            // in ejs-array.c / test/slice-negative1.js)
            plainParams = plainParams.slice(0, plainParams.length - 1);
        }
        for (let p of plainParams) {
            if (p.type !== "Identifier")
                throw LowerNotSupported(`param pattern ${p.type}`, fnNode.loc);
            let binding = this.curScope!.declare(p.name, "param");
            info.params.push(binding);
        }
        info.restBinding = null;
        if (restId) {
            if (restId.type !== "Identifier")
                throw LowerNotSupported(`rest pattern ${restId.type}`, fnNode.loc);
            info.restBinding = this.curScope!.declare(restId.name, "local");
            this.refs.set(restId, info.restBinding);
        }
        // default-parameter expressions are evaluated in the function scope
        // (all params are declared, matching the sequential leftward-only
        // visibility of the legacy DesugarDefaults lowering)
        info.defaults = fnNode.defaults || [];
        for (let d of info.defaults) {
            if (d) this.walkExpr(d);
        }
        return info;
    }

    leaveFunction(): void {
        this.curScope = this.curScope!.parent;
        this.curFn = this.curFn!.parent;
        this.labelStack = this.savedLabelStacks.pop()!;
    }

    pushLoopEnv(node: e.Node): LoopEnv {
        let top = this.loopEnvStack[this.loopEnvStack.length - 1];
        let parentCandidate = top && top.fnInfo === this.curFn ? top : null;
        let le = new LoopEnv(this.curFn, node, parentCandidate);
        this.loopEnvs.push(le);
        this.loopEnvByNode.set(node, le);
        this.loopEnvStack.push(le);
        return le;
    }

    // the BODY env of a loop: captured let/const declared anywhere in the
    // loop body (at any block depth, in the same function) get a fresh
    // environment per iteration — their declarations re-execute each pass,
    // so no value copies forward (unlike for-head vars).  pushed around the
    // body walk of every loop form.
    pushLoopBodyEnv(node: e.Node): LoopEnv {
        let top = this.loopEnvStack[this.loopEnvStack.length - 1];
        let parentCandidate = top && top.fnInfo === this.curFn ? top : null;
        let le = new LoopEnv(this.curFn, node, parentCandidate);
        this.loopEnvs.push(le);
        this.bodyEnvByNode.set(node, le);
        this.loopEnvStack.push(le);
        return le;
    }

    // a let/const declaration inside a loop body attaches to that loop's
    // body env (top of stack, same function)
    attachBodyLet(binding: Binding): void {
        let top = this.loopEnvStack[this.loopEnvStack.length - 1];
        if (!top || top.fnInfo !== this.curFn) return;
        binding.loopEnv = top;
        top.allBindings.push(binding);
    }

    reference(idNode: e.Identifier, isCallee = false): Binding | null {
        if (idNode.name === "undefined") {
            this.refs.set(idNode, null);
            return null;
        }
        if (idNode.name === "arguments") {
            // bind to the nearest non-arrow function's (synthetic)
            // arguments object, created in its prologue
            let f = this.curFn;
            while (f && f.node.type === "ArrowFunctionExpression") f = f.parent;
            if (!f) throw LowerNotSupported("`arguments` outside a function", idNode.loc);
            if (!f.argumentsBinding) {
                f.argumentsBinding = new Binding("arguments", "local", f);
                f.bindings.push(f.argumentsBinding);
                f.usesArguments = true;
            }
            let abinding = f.argumentsBinding;
            this.refs.set(idNode, abinding);
            if (abinding.fnInfo !== this.curFn) {
                abinding.captured = true;
                let g = this.curFn;
                while (g && g !== abinding.fnInfo) {
                    g.needsParentEnv = true;
                    g = g.parent;
                }
            }
            return abinding;
        }
        let binding = this.curScope!.lookup(idNode.name);
        this.refs.set(idNode, binding); // null = global
        if (!binding) {
            // %-named identifiers are compiler-synthesized: ones that
            // resolve to bindings (%super, pattern temps) are fine, but an
            // unresolved one is a legacy-intrinsic shape lowering doesn't
            // model (e.g. `%constructSuper.apply(...)` from a spread super
            // call) — never a real global.  fall back, don't miscompile.
            if (idNode.name[0] === "%")
                throw LowerNotSupported(`unresolved %-identifier ${idNode.name}`, idNode.loc);
            this.globalNames.add(idNode.name);
            if (!isCallee) this.globalValueNames.add(idNode.name);
            return null;
        }

        if (binding.kind === "self") {
            // direct recursion from the function itself stays a direct
            // call.  anything else (value-position uses, references from
            // nested functions) is treated as a free module-scope name:
            // integration resolves it through the module slot when the
            // candidate is exported or promoted, and falls back otherwise.
            if (binding.fnInfo !== this.curFn || !isCallee) {
                this.refs.set(idNode, null);
                this.globalNames.add(idNode.name);
                if (!isCallee) this.globalValueNames.add(idNode.name);
                return null;
            }
            return binding;
        }

        if (binding.fnInfo !== this.curFn) {
            binding.captured = true;
            // every function on the chain between the reference and the
            // declaration needs access to its parent's environment
            let f = this.curFn;
            while (f && f !== binding.fnInfo) {
                f.needsParentEnv = true;
                f = f.parent;
            }
        }
        return binding;
    }

    // --- statements ---------------------------------------------------------------

    walkStmt(n: e.Statement): void {
        switch (n.type) {
            case "BlockStatement": {
                this.curScope = new LexScope(this.curScope, this.curFn);
                for (const s of n.body) this.walkStmt(s);
                this.curScope = this.curScope!.parent;
                return;
            }
            case "VariableDeclaration": {
                // consume the for-init loop env candidate before descending
                // into initializer expressions (a nested function's own
                // declarations must not attach to it)
                let ple = this.pendingLoopEnv;
                this.pendingLoopEnv = null;
                for (let d of n.declarations) {
                    if (d.id.type === "ObjectPattern") {
                        this.declareObjectPattern(n, d, ple);
                        continue;
                    }
                    if (d.id.type !== "Identifier")
                        throw LowerNotSupported(`declaration pattern ${d.id.type}`, n.loc);
                    // declare BEFORE walking the init: a closure created in
                    // the initializer must see the binding (`let walk =
                    // (n) => ... walk(n) ...`), or its recursive reference
                    // silently resolves to a global.  a direct `let x = x`
                    // reads the pre-initialized undefined (lower.js writes
                    // undefined before evaluating the init), matching the
                    // legacy alloca behavior.
                    // var declarations hoist to the function scope; only
                    // let/const are block-scoped.
                    let scope = this.curScope!;
                    if (n.kind === "var") {
                        while (!scope.isFnTop) scope = scope.parent!;
                    }
                    if (this.slotBackedDecl(d.id.name, scope)) {
                        // toplevel module binding: no local; the declarator
                        // lowers as a slot store, references via refs
                        if (d.init) this.walkExpr(d.init);
                        continue;
                    }
                    let binding = scope.declare(d.id.name, "local");
                    this.refs.set(d.id, binding);
                    if (ple) {
                        binding.loopEnv = ple;
                        ple.allBindings.push(binding);
                    } else if (n.kind !== "var") {
                        // a let/const inside a loop body: fresh binding per
                        // iteration if captured
                        this.attachBodyLet(binding);
                    }
                    if (d.init) this.walkExpr(d.init);
                }
                return;
            }
            case "FunctionDeclaration": {
                if (!n.id) throw LowerNotSupported("unnamed function declaration", n.loc);
                if (!this.curScope!.isFnTop)
                    throw LowerNotSupported("block-level function declaration", n.loc);
                if (this.slotBackedDecl(n.id.name, this.curScope!)) {
                    // toplevel module function: no local binding — the
                    // closure is stored to its slot at this statement's
                    // position, and every reference (self-references
                    // included) reads the slot
                    let fname = `${this.curFn!.name}.${n.id.name}`;
                    this.enterFunction(n, fname);
                    this.walkFnBody(n.body);
                    this.leaveFunction();
                    return;
                }
                // the walkFnBody prescan already declared this name;
                // declare() hands back the same binding.  genuine
                // same-scope duplicates can't survive HoistFuncDecls
                // (its per-name map keeps only the last declaration).
                let binding = this.curScope!.declare(n.id.name, "fn");
                this.refs.set(n.id, binding);
                let name = this.curFn ? `${this.curFn.name}.${n.id.name}` : n.id.name;
                this.enterFunction(n, name);
                this.walkFnBody(n.body);
                this.leaveFunction();
                return;
            }
            case "ImportDeclaration": {
                // toplevel mode only: scaffolding resolves the imported
                // module; binding reads route through refs.  a specifier
                // whose local name has no slot backing (a native module's
                // named import) keeps the module on the legacy path.
                if (this.moduleSlotNames === null || this.curFn !== this.rootInfo)
                    throw LowerNotSupported("import declaration", n.loc);
                for (let spec of n.specifiers) {
                    let local = spec.local || spec.id;
                    if (!local || !this.moduleSlotNames.has(local.name))
                        throw LowerNotSupported(
                            `import binding '${local && local.name}' has no slot`,
                            n.loc
                        );
                }
                return;
            }
            case "ExportNamedDeclaration": {
                if (this.moduleSlotNames === null || this.curFn !== this.rootInfo)
                    throw LowerNotSupported("export declaration", n.loc);
                // re-export (`export { a as b } from "m"`): the specifier
                // names are the SOURCE module's exports, not local
                // references — nothing to resolve here (lowering validates
                // them against the source's export table)
                if (n.source) return;
                if (n.declaration && !Array.isArray(n.declaration))
                    return this.walkStmt(n.declaration);
                if (n.specifiers && n.specifiers.length > 0) {
                    for (let spec of n.specifiers) this.walkExpr(spec.local);
                    return;
                }
                // `export {}` — a valid, empty statement
                return;
            }
            case "ExportDefaultDeclaration": {
                if (this.moduleSlotNames === null || this.curFn !== this.rootInfo)
                    throw LowerNotSupported("export default", n.loc);
                if (
                    n.declaration.type === "FunctionDeclaration" ||
                    n.declaration.type === "ClassDeclaration"
                )
                    throw LowerNotSupported("export default declaration", n.loc);
                this.walkExpr(n.declaration as e.Expression);
                return;
            }
            case "ExportAllDeclaration":
                // both forms name only the SOURCE module's exports —
                // nothing local to resolve (lowering validates them
                // against the source's export table)
                if (this.moduleSlotNames === null || this.curFn !== this.rootInfo)
                    throw LowerNotSupported("export declaration", n.loc);
                return;
            case "ExpressionStatement":
                this.walkExpr(n.expression);
                return;
            case "IfStatement":
                this.walkExpr(n.test);
                this.walkStmt(n.consequent);
                if (n.alternate) this.walkStmt(n.alternate);
                return;
            case "WhileStatement": {
                this.walkExpr(n.test);
                this.pushLoopBodyEnv(n);
                this.walkStmt(n.body);
                this.loopEnvStack.pop();
                return;
            }
            case "DoWhileStatement": {
                this.pushLoopBodyEnv(n);
                this.walkStmt(n.body);
                this.loopEnvStack.pop();
                this.walkExpr(n.test);
                return;
            }
            case "ForStatement": {
                this.curScope = new LexScope(this.curScope, this.curFn);
                let le = null;
                if (n.init && n.init.type === "VariableDeclaration" && n.init.kind !== "var") {
                    le = this.pushLoopEnv(n);
                }
                if (n.init) {
                    if (n.init.type === "VariableDeclaration") {
                        // the declared bindings attach to the loop env
                        // candidate (cleared by the declaration walk before
                        // it descends into initializer expressions)
                        this.pendingLoopEnv = le;
                        this.walkStmt(n.init);
                        this.pendingLoopEnv = null;
                    } else this.walkExpr(n.init);
                }
                if (n.test) this.walkExpr(n.test);
                if (n.update) this.walkExpr(n.update);
                this.pushLoopBodyEnv(n);
                this.walkStmt(n.body);
                this.loopEnvStack.pop();
                if (le) this.loopEnvStack.pop();
                this.curScope = this.curScope!.parent;
                return;
            }
            case "ForInStatement":
            case "ForOfStatement": {
                this.curScope = new LexScope(this.curScope, this.curFn);
                let le = null;
                if (n.left.type === "VariableDeclaration") {
                    const d = n.left.declarations[0];
                    if (n.left.declarations.length !== 1 || !d || d.id.type !== "Identifier" || d.init)
                        throw LowerNotSupported("for-of/for-in binding form", n.loc);
                    let scope = this.curScope!;
                    if (n.left.kind === "var") {
                        while (!scope.isFnTop) scope = scope.parent!;
                    }
                    const binding = scope.declare(d.id.name, "local");
                    this.refs.set(d.id, binding);
                    if (n.left.kind !== "var") {
                        le = this.pushLoopEnv(n);
                        binding.loopEnv = le;
                        le.allBindings.push(binding);
                    }
                } else if (n.left.type === "Identifier") {
                    let binding = this.reference(n.left);
                    if (!binding) this.globalAssignedNames.add(n.left.name);
                } else {
                    throw LowerNotSupported(`for-of/for-in target ${n.left.type}`, n.loc);
                }
                this.walkExpr(n.right);
                this.pushLoopBodyEnv(n);
                this.walkStmt(n.body);
                this.loopEnvStack.pop();
                if (le) this.loopEnvStack.pop();
                this.curScope = this.curScope!.parent;
                return;
            }
            case "SwitchStatement": {
                this.walkExpr(n.discriminant);
                // all case bodies share one lexical scope
                this.curScope = new LexScope(this.curScope, this.curFn);
                let sawDefault = false;
                for (let c of n.cases) {
                    if (!c.test) {
                        if (sawDefault)
                            throw LowerNotSupported("duplicate default case", n.loc);
                        sawDefault = true;
                    } else {
                        this.walkExpr(c.test);
                    }
                    for (let s of c.consequent) this.walkStmt(s);
                }
                this.curScope = this.curScope!.parent;
                return;
            }
            case "ReturnStatement":
                if (n.argument) this.walkExpr(n.argument);
                return;
            case "ThrowStatement":
                this.walkExpr(n.argument);
                return;
            case "TryStatement": {
                let nhandlers = n.handlers ? n.handlers.length : 0;
                if (nhandlers > 1)
                    throw LowerNotSupported("try with multiple catch clauses", n.loc);
                if (nhandlers === 0 && !n.finalizer)
                    throw LowerNotSupported("try without catch or finally", n.loc);
                this.walkStmt(n.block);
                if (nhandlers === 1) {
                    const handler = n.handlers[0]!;
                    this.curScope = new LexScope(this.curScope, this.curFn);
                    if (handler.param) {
                        if (handler.param.type !== "Identifier")
                            throw LowerNotSupported("catch parameter pattern", n.loc);
                        let binding = this.curScope!.declare(handler.param.name, "catch");
                        this.refs.set(handler.param, binding);
                    }
                    this.walkStmt(handler.body);
                    this.curScope = this.curScope!.parent;
                }
                if (n.finalizer) this.walkStmt(n.finalizer);
                return;
            }
            case "LabeledStatement": {
                if (this.labelStack.some((l) => l.name === n.label.name))
                    throw LowerNotSupported(`duplicate label '${n.label.name}'`, n.loc);
                // a label chain ending in a loop is continue-able
                let body = n.body;
                while (body.type === "LabeledStatement") body = body.body;
                let isLoop =
                    body.type === "WhileStatement" ||
                    body.type === "DoWhileStatement" ||
                    body.type === "ForStatement" ||
                    body.type === "ForInStatement" ||
                    body.type === "ForOfStatement";
                this.labelStack.push({ name: n.label.name, isLoop: isLoop });
                this.walkStmt(n.body);
                this.labelStack.pop();
                return;
            }
            case "BreakStatement":
                if (n.label) {
                    const labelName = n.label.name;
                    const l = this.labelStack.find((x) => x.name === labelName);
                    if (!l) throw LowerNotSupported(`break to unknown label '${labelName}'`, n.loc);
                }
                return;
            case "ContinueStatement":
                if (n.label) {
                    const labelName = n.label.name;
                    const l = this.labelStack.find((x) => x.name === labelName);
                    if (!l || !l.isLoop)
                        throw LowerNotSupported(`continue to non-loop label '${labelName}'`, n.loc);
                }
                return;
            case "EmptyStatement":
            case "DebuggerStatement": // a no-op in compiled code
                return;
            default:
                throw LowerNotSupported(`statement type ${n.type}`, n.loc);
        }
    }

    // `let { a, b: c, d = dflt } = init` — shallow object patterns only.
    // loopEnv is the enclosing for-init loop env candidate, if any.
    declareObjectPattern(declStmt: e.VariableDeclaration, d: e.VariableDeclarator, loopEnv: LoopEnv | null): void {
        let scope = this.curScope!;
        if (declStmt.kind === "var") {
            while (!scope.isFnTop) scope = scope.parent!;
        }
        for (const prop of (d.id as e.ObjectPattern).properties) {
            if (prop.type === "RestElement")
                throw LowerNotSupported("rest property in declaration pattern", declStmt.loc);
            if (prop.computed)
                throw LowerNotSupported("computed key in declaration pattern", declStmt.loc);
            if (prop.key.type !== "Identifier" && prop.key.type !== "Literal")
                throw LowerNotSupported("declaration pattern key", declStmt.loc);
            let target = prop.value as e.Pattern;
            let dflt: e.Expression | null = null;
            if (target.type === "AssignmentPattern") {
                dflt = target.right;
                target = target.left;
            }
            if (target.type !== "Identifier")
                throw LowerNotSupported(
                    `nested declaration pattern ${target.type}`,
                    declStmt.loc
                );
            let binding = scope.declare(target.name, "local");
            this.refs.set(target, binding);
            if (loopEnv) {
                binding.loopEnv = loopEnv;
                loopEnv.allBindings.push(binding);
            }
            if (dflt) this.walkExpr(dflt);
        }
        if (d.init) this.walkExpr(d.init);
    }

    // --- expressions ------------------------------------------------------------

    walkExpr(n: e.Expression | e.SpreadElement): void {
        switch (n.type) {
            case "Literal":
                // object-valued literals are regexes (lowerable) or
                // engine-specific oddities (fall back early)
                if (n.value !== null && typeof n.value === "object") {
                    if (typeof n.value.source !== "string")
                        throw LowerNotSupported(`literal ${typeof n.value}`, n.loc);
                }
                return;
            case "Identifier":
                this.reference(n);
                return;
            case "BinaryExpression":
            case "LogicalExpression":
                this.walkExpr(n.left as e.Expression);
                this.walkExpr(n.right);
                return;
            case "UnaryExpression":
                if (n.operator === "delete" && n.argument.type !== "MemberExpression")
                    throw LowerNotSupported("delete of a non-member expression", n.loc);
                this.walkExpr(n.argument);
                return;
            case "AssignmentExpression":
                // compound assignments must desugar to a binop lowering
                // knows; reject others here so we fall back early (a late
                // lowering failure abandons the whole file's EIR set)
                if (n.operator !== "=" && !compound_assign_ops[n.operator])
                    throw LowerNotSupported(`assignment operator ${n.operator}`, n.loc);
                if (n.left.type === "Identifier") {
                    let binding = this.reference(n.left);
                    if (!binding) this.globalAssignedNames.add(n.left.name);
                } else this.walkExpr(n.left as e.Expression);
                this.walkExpr(n.right);
                return;
            case "UpdateExpression":
                if (n.argument.type === "Identifier") {
                    let binding = this.reference(n.argument);
                    if (!binding) this.globalAssignedNames.add(n.argument.name);
                } else if (n.argument.type === "MemberExpression") {
                    this.walkExpr(n.argument);
                } else {
                    throw LowerNotSupported(`update of ${n.argument.type}`, n.loc);
                }
                return;
            case "TemplateLiteral":
                for (let e of n.expressions) this.walkExpr(e);
                return;
            case "TaggedTemplateExpression":
                if (n.tag.type === "Identifier") this.reference(n.tag, true);
                else this.walkExpr(n.tag);
                for (let e of n.quasi.expressions) this.walkExpr(e);
                return;
            case "CallExpression":
                // %-intrinsic calls (from the pre-EIR desugar passes):
                // the callee is a lowering directive, not a reference.
                // only whitelisted intrinsics lower; reject others early.
                if (n.callee.type === "Identifier" && n.callee.name[0] === "%") {
                    if (!eir_intrinsics[n.callee.name])
                        throw LowerNotSupported(`intrinsic ${n.callee.name}`, n.loc);
                    for (let a of n.arguments) this.walkExpr(a);
                    return;
                }
                if (n.callee.type === "Identifier") this.reference(n.callee, true);
                else this.walkExpr(n.callee);
                for (let a of n.arguments) this.walkExpr(a);
                return;
            case "NewExpression":
                this.walkExpr(n.callee);
                for (let a of n.arguments) this.walkExpr(a);
                return;
            case "MemberExpression":
                this.walkExpr(n.object);
                if (n.computed) this.walkExpr(n.property as e.Expression);
                return;
            case "ConditionalExpression":
                this.walkExpr(n.test);
                this.walkExpr(n.consequent);
                this.walkExpr(n.alternate);
                return;
            case "FunctionExpression": {
                let name = (n.id && n.id.name) || `anon${this.anon_gen++}`;
                this.enterFunction(n, this.curFn ? `${this.curFn.name}.${name}` : name);
                this.walkFnBody(n.body);
                this.leaveFunction();
                return;
            }
            case "ArrowFunctionExpression": {
                // arrows lower as ordinary closures; lexical `this` reads
                // resolve to the owner function's captured this binding
                // (see the ThisExpression case below)
                let name = `arrow${this.anon_gen++}`;
                this.enterFunction(n, this.curFn ? `${this.curFn.name}.${name}` : name);
                if (n.body.type === "BlockStatement") this.walkFnBody(n.body);
                else this.walkExpr(n.body);
                this.leaveFunction();
                return;
            }
            case "ThisExpression": {
                // an arrow's `this` is lexical: capture the nearest
                // non-arrow ancestor's this in its environment (the same
                // shape as the `arguments` machinery above)
                let f = this.curFn;
                while (f && f.node.type === "ArrowFunctionExpression") f = f.parent;
                // sloppy-mode functions coerce a null/undefined `this` to
                // the global object at entry; record the use so lowering
                // only pays for it where `this` is actually read
                if (f) f.usesThis = true;
                // a candidate whose root IS an arrow has no owner here;
                // its lexical `this` is the module toplevel's — fall back
                if (!f) throw LowerNotSupported("lexical `this` in a toplevel arrow", n.loc);
                if (f !== this.curFn) {
                    if (!f.thisBinding) {
                        f.thisBinding = new Binding("%this", "this", f);
                        f.bindings.push(f.thisBinding);
                    }
                    f.thisBinding.captured = true;
                    this.refs.set(n, f.thisBinding);
                    let g = this.curFn;
                    while (g && g !== f) {
                        g.needsParentEnv = true;
                        g = g.parent;
                    }
                }
                return;
            }
            case "SequenceExpression":
                for (let e of n.expressions) this.walkExpr(e);
                return;
            case "ArrayExpression":
                for (let e of n.elements) if (e) this.walkExpr(e);
                return;
            case "ObjectExpression":
                for (const p of n.properties) {
                    if (p.type === "SpreadElement") {
                        this.walkExpr(p.argument);
                        continue;
                    }
                    if (p.computed) this.walkExpr(p.key);
                    this.walkExpr(p.value as e.Expression);
                }
                return;
            default:
                throw LowerNotSupported(`expression type ${n.type}`, n.loc);
        }
    }
}

// assign env slots for `info` and every function below it
function assignSlots(info: FnInfo): void {
    let next = 0;
    // a parent pointer is only needed in the env if this function actually
    // allocates one; if it doesn't, its incoming env already *is* the parent.
    // captured bindings living in a per-iteration loop env got their slots
    // there (analyzeFunction) and don't occupy function-env slots.
    let captured = info.bindings.filter(
        (bd) => bd.captured && !(bd.loopEnv && bd.loopEnv.materialized)
    );
    let wantsEnv = captured.length > 0;
    if (wantsEnv && info.needsParentEnv && info.parent !== null) {
        info.parentSlot = next++;
    }
    for (let bd of captured) bd.slot = next++;
    info.envSize = next;

    for (let child of info.children) assignSlots(child);
}
