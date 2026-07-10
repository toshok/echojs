/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

// Scope analysis for EIR lowering.  A fresh, self-contained replacement for
// the parts of new-cc's Scope/Binding machinery that lowering needs:
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
// lower.js and throws LowerNotSupported on anything else, so a function
// either lowers completely or falls back to the legacy path as a unit.

import * as b from "../ast-builder";
import { LowerNotSupported, isLowerNotSupported } from "./errors";
import { eir_intrinsics } from "./intrinsics";

// compound assignment operator -> the binary operator it desugars to
// (kept in sync with lower.js's binops table)
export const compound_assign_ops = {
    "+=": "+",
    "-=": "-",
    "*=": "*",
    "/=": "/",
    "%=": "%",
    "&=": "&",
    "|=": "|",
    "^=": "^",
    "<<=": "<<",
    ">>=": ">>",
    ">>>=": ">>>",
};

let binding_id_gen = 0;

export class Binding {
    constructor(name, kind, fnInfo) {
        this.name = name;
        this.uid = `${name}#${binding_id_gen++}`;
        this.kind = kind; // "param" | "local" | "fn" | "catch"
        this.fnInfo = fnInfo; // declaring FnInfo
        this.captured = false;
        this.slot = -1; // env slot, if captured
        this.loopEnv = null; // LoopEnv candidate, for let/const loop bindings
    }
}

export class FnInfo {
    constructor(node, name, parent) {
        this.node = node;
        this.name = name;
        this.parent = parent; // FnInfo or null
        this.children = [];
        this.params = []; // Binding[]
        this.bindings = []; // every Binding declared here
        this.needsParentEnv = false; // some descendant reaches past this fn
        this.envSize = 0; // slots (incl. parent slot), 0 = no env
        this.parentSlot = -1; // slot holding the parent env, or -1
        this.creationLoopEnv = null; // innermost LoopEnv at the definition site
        if (parent) parent.children.push(this);
    }
}

// a per-iteration environment for a loop whose let/const bindings are
// captured by closures (`for (let i ...) { use(() => i); }`): each
// iteration allocates a fresh env so every closure sees that iteration's
// binding.  slot 0 always holds the enclosing environment (the value of
// curEnv at loop entry).  candidates are created for every let/const
// loop declaration during the walk and materialize after it, once
// capture flags are known; unmaterialized candidates are transparent.
let loopenv_id_gen = 0;

export class LoopEnv {
    constructor(fnInfo, node, parentCandidate) {
        this.id = loopenv_id_gen++;
        this.isLoopEnv = true;
        this.fnInfo = fnInfo; // the function containing the loop
        this.node = node; // the loop AST node
        this.parentCandidate = parentCandidate; // enclosing LoopEnv in the same fn, or null
        this.allBindings = []; // every let/const binding the loop declares
        this.bindings = []; // the captured subset (set at materialization)
        this.materialized = false;
        this.envSize = 0;
        this.parentSlot = -1; // always 0 once materialized
    }
}

class LexScope {
    constructor(parent, fnInfo) {
        this.parent = parent;
        this.fnInfo = fnInfo;
        this.names = new Map(); // name -> Binding
    }

    declare(name, kind) {
        // redeclaration in the same lexical scope reuses the binding (var x;
        // var x; — and function-level var hoisting lands them in one scope)
        if (this.names.has(name)) return this.names.get(name);
        let binding = new Binding(name, kind, this.fnInfo);
        this.names.set(name, binding);
        this.fnInfo.bindings.push(binding);
        return binding;
    }

    lookup(name) {
        let s = this;
        while (s) {
            if (s.names.has(name)) return s.names.get(name);
            s = s.parent;
        }
        return null;
    }
}

export class ScopeAnalysis {
    constructor() {
        this.refs = new Map(); // Identifier node -> Binding | null (global)
        this.fnInfos = new Map(); // Function node -> FnInfo
        this.globalNames = new Set(); // free names that resolved to nothing
        this.globalValueNames = new Set(); // free names used other than as a direct callee
        this.globalAssignedNames = new Set(); // free names that are assigned to
        this.anon_gen = 0;
        this.curScope = null;
        this.curFn = null;
        // per-iteration loop env candidates: every let/const loop
        // declaration gets one; those with captured bindings materialize
        // after the walk (see analyzeFunction) and lowering builds a
        // fresh env per iteration.
        this.loopEnvs = [];
        this.loopEnvStack = []; // active candidates (innermost last)
        this.loopEnvByNode = new Map(); // loop AST node -> LoopEnv
        // set around a for-init declaration walk so the declared bindings
        // attach to the loop's env candidate
        this.pendingLoopEnv = null;
    }

    // the loop's materialized env, or null (for lowering)
    loopEnvOf(node) {
        let le = this.loopEnvByNode.get(node);
        return le && le.materialized ? le : null;
    }

    resolve(node) {
        return this.refs.get(node);
    }

    infoFor(fnNode) {
        return this.fnInfos.get(fnNode);
    }

    // --- entry point ---------------------------------------------------------

    // walk a function's body without pushing a block scope: the body's
    // top-level declarations belong to the function scope itself (isFnTop),
    // otherwise every body-level function declaration would look like a
    // block-level one.
    walkFnBody(body) {
        for (let s of body.body) this.walkStmt(s);
    }

    analyzeFunction(fnNode, name) {
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
        if (fnNode.body.type === b.BlockStatement) this.walkFnBody(fnNode.body);
        else this.walkExpr(fnNode.body); // expression-bodied arrow
        this.leaveFunction();
        if (selfBinding) this.curScope = this.curScope.parent;
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
        return info;
    }

    enterFunction(fnNode, name) {
        let fname = name || (fnNode.id && fnNode.id.name) || "anon";
        let info = new FnInfo(fnNode, fname, this.curFn);
        this.fnInfos.set(fnNode, info);

        // the innermost loop env active at this definition site (in the
        // DEFINING function): the closure's incoming env is that loop's
        // per-iteration env, so env-chain walks must start there
        let leTop = this.loopEnvStack[this.loopEnvStack.length - 1];
        info.creationLoopEnv = leTop && leTop.fnInfo === this.curFn ? leTop : null;

        if (fnNode.generator)
            throw LowerNotSupported("generator function", fnNode.loc);

        this.curFn = info;
        this.curScope = new LexScope(this.curScope, info);
        this.curScope.isFnTop = true;
        // the rest parameter (a trailing RestElement, or fnNode.rest in
        // older ASTs) is an ordinary local initialized from the trailing
        // arguments in the prologue (see lower.js / rest_args)
        let restId = fnNode.rest || null;
        let plainParams = fnNode.params;
        let last = plainParams[plainParams.length - 1];
        if (last && last.type === b.RestElement) {
            restId = last.argument;
            // (positive end index: the self-hosted runtime's slice-dense
            // fast path crashes on negative indices — see runtime bug note
            // in ejs-array.c / test/slice-negative1.js)
            plainParams = plainParams.slice(0, plainParams.length - 1);
        }
        for (let p of plainParams) {
            if (p.type !== b.Identifier)
                throw LowerNotSupported(`param pattern ${p.type}`, fnNode.loc);
            let binding = this.curScope.declare(p.name, "param");
            info.params.push(binding);
        }
        info.restBinding = null;
        if (restId) {
            if (restId.type !== b.Identifier)
                throw LowerNotSupported(`rest pattern ${restId.type}`, fnNode.loc);
            info.restBinding = this.curScope.declare(restId.name, "local");
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

    leaveFunction() {
        this.curScope = this.curScope.parent;
        this.curFn = this.curFn.parent;
    }

    pushLoopEnv(node) {
        let top = this.loopEnvStack[this.loopEnvStack.length - 1];
        let parentCandidate = top && top.fnInfo === this.curFn ? top : null;
        let le = new LoopEnv(this.curFn, node, parentCandidate);
        this.loopEnvs.push(le);
        this.loopEnvByNode.set(node, le);
        this.loopEnvStack.push(le);
        return le;
    }

    reference(idNode, isCallee) {
        if (idNode.name === "undefined") {
            this.refs.set(idNode, null);
            return null;
        }
        if (idNode.name === "arguments") {
            // bind to the nearest non-arrow function's (synthetic)
            // arguments object, created in its prologue
            let f = this.curFn;
            while (f && f.node.type === b.ArrowFunctionExpression) f = f.parent;
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
        let binding = this.curScope.lookup(idNode.name);
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

    walkStmt(n) {
        switch (n.type) {
            case b.BlockStatement: {
                this.curScope = new LexScope(this.curScope, this.curFn);
                for (let s of n.body) this.walkStmt(s);
                this.curScope = this.curScope.parent;
                return;
            }
            case b.VariableDeclaration: {
                // consume the for-init loop env candidate before descending
                // into initializer expressions (a nested function's own
                // declarations must not attach to it)
                let ple = this.pendingLoopEnv;
                this.pendingLoopEnv = null;
                for (let d of n.declarations) {
                    if (d.id.type === b.ObjectPattern) {
                        this.declareObjectPattern(n, d, ple);
                        continue;
                    }
                    if (d.id.type !== b.Identifier)
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
                    let scope = this.curScope;
                    if (n.kind === "var") {
                        while (!scope.isFnTop) scope = scope.parent;
                    }
                    let binding = scope.declare(d.id.name, "local");
                    this.refs.set(d.id, binding);
                    if (ple) {
                        binding.loopEnv = ple;
                        ple.allBindings.push(binding);
                    }
                    if (d.init) this.walkExpr(d.init);
                }
                return;
            }
            case b.FunctionDeclaration: {
                if (!n.id) throw LowerNotSupported("unnamed function declaration", n.loc);
                if (!this.curScope.isFnTop)
                    throw LowerNotSupported("block-level function declaration", n.loc);
                if (this.curScope.names.has(n.id.name))
                    throw LowerNotSupported(
                        `redeclaration of function '${n.id.name}'`,
                        n.loc
                    );
                let binding = this.curScope.declare(n.id.name, "fn");
                this.refs.set(n.id, binding);
                let name = this.curFn ? `${this.curFn.name}.${n.id.name}` : n.id.name;
                this.enterFunction(n, name);
                this.walkFnBody(n.body);
                this.leaveFunction();
                return;
            }
            case b.ExpressionStatement:
                this.walkExpr(n.expression);
                return;
            case b.IfStatement:
                this.walkExpr(n.test);
                this.walkStmt(n.consequent);
                if (n.alternate) this.walkStmt(n.alternate);
                return;
            case b.WhileStatement:
                this.walkExpr(n.test);
                this.walkStmt(n.body);
                return;
            case b.DoWhileStatement:
                this.walkStmt(n.body);
                this.walkExpr(n.test);
                return;
            case b.ForStatement: {
                this.curScope = new LexScope(this.curScope, this.curFn);
                let le = null;
                if (n.init && n.init.type === b.VariableDeclaration && n.init.kind !== "var") {
                    le = this.pushLoopEnv(n);
                }
                if (n.init) {
                    if (n.init.type === b.VariableDeclaration) {
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
                this.walkStmt(n.body);
                if (le) this.loopEnvStack.pop();
                this.curScope = this.curScope.parent;
                return;
            }
            case b.ForInStatement:
            case b.ForOfStatement: {
                this.curScope = new LexScope(this.curScope, this.curFn);
                let le = null;
                if (n.left.type === b.VariableDeclaration) {
                    if (
                        n.left.declarations.length !== 1 ||
                        n.left.declarations[0].id.type !== b.Identifier ||
                        n.left.declarations[0].init
                    )
                        throw LowerNotSupported("for-of/for-in binding form", n.loc);
                    let d = n.left.declarations[0];
                    let scope = this.curScope;
                    if (n.left.kind === "var") {
                        while (!scope.isFnTop) scope = scope.parent;
                    }
                    let binding = scope.declare(d.id.name, "local");
                    this.refs.set(d.id, binding);
                    if (n.left.kind !== "var") {
                        le = this.pushLoopEnv(n);
                        binding.loopEnv = le;
                        le.allBindings.push(binding);
                    }
                } else if (n.left.type === b.Identifier) {
                    let binding = this.reference(n.left);
                    if (!binding) this.globalAssignedNames.add(n.left.name);
                } else {
                    throw LowerNotSupported(`for-of/for-in target ${n.left.type}`, n.loc);
                }
                this.walkExpr(n.right);
                this.walkStmt(n.body);
                if (le) this.loopEnvStack.pop();
                this.curScope = this.curScope.parent;
                return;
            }
            case b.SwitchStatement: {
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
                this.curScope = this.curScope.parent;
                return;
            }
            case b.ReturnStatement:
                if (n.argument) this.walkExpr(n.argument);
                return;
            case b.ThrowStatement:
                this.walkExpr(n.argument);
                return;
            case b.TryStatement: {
                let nhandlers = n.handlers ? n.handlers.length : 0;
                if (nhandlers > 1)
                    throw LowerNotSupported("try with multiple catch clauses", n.loc);
                if (nhandlers === 0 && !n.finalizer)
                    throw LowerNotSupported("try without catch or finally", n.loc);
                this.walkStmt(n.block);
                if (nhandlers === 1) {
                    let handler = n.handlers[0];
                    this.curScope = new LexScope(this.curScope, this.curFn);
                    if (handler.param) {
                        if (handler.param.type !== b.Identifier)
                            throw LowerNotSupported("catch parameter pattern", n.loc);
                        let binding = this.curScope.declare(handler.param.name, "catch");
                        this.refs.set(handler.param, binding);
                    }
                    this.walkStmt(handler.body);
                    this.curScope = this.curScope.parent;
                }
                if (n.finalizer) this.walkStmt(n.finalizer);
                return;
            }
            case b.BreakStatement:
            case b.ContinueStatement:
                if (n.label) throw LowerNotSupported("labeled break/continue", n.loc);
                return;
            case b.EmptyStatement:
                return;
            default:
                throw LowerNotSupported(`statement type ${n.type}`, n.loc);
        }
    }

    // `let { a, b: c, d = dflt } = init` — shallow object patterns only.
    // loopEnv is the enclosing for-init loop env candidate, if any.
    declareObjectPattern(declStmt, d, loopEnv) {
        let scope = this.curScope;
        if (declStmt.kind === "var") {
            while (!scope.isFnTop) scope = scope.parent;
        }
        for (let prop of d.id.properties) {
            if (prop.computed)
                throw LowerNotSupported("computed key in declaration pattern", declStmt.loc);
            if (prop.key.type !== b.Identifier && prop.key.type !== b.Literal)
                throw LowerNotSupported("declaration pattern key", declStmt.loc);
            let target = prop.value;
            let dflt = null;
            if (target.type === b.AssignmentPattern) {
                dflt = target.right;
                target = target.left;
            }
            if (target.type !== b.Identifier)
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

    walkExpr(n) {
        switch (n.type) {
            case b.Literal:
                // object-valued literals are regexes (lowerable) or
                // engine-specific oddities (fall back early)
                if (n.value !== null && typeof n.value === "object") {
                    if (typeof n.value.source !== "string")
                        throw LowerNotSupported(`literal ${typeof n.value}`, n.loc);
                }
                return;
            case b.Identifier:
                this.reference(n);
                return;
            case b.BinaryExpression:
            case b.LogicalExpression:
                this.walkExpr(n.left);
                this.walkExpr(n.right);
                return;
            case b.UnaryExpression:
                if (n.operator === "delete" && n.argument.type !== b.MemberExpression)
                    throw LowerNotSupported("delete of a non-member expression", n.loc);
                this.walkExpr(n.argument);
                return;
            case b.AssignmentExpression:
                // compound assignments must desugar to a binop lowering
                // knows; reject others here so we fall back early (a late
                // lowering failure abandons the whole file's EIR set)
                if (n.operator !== "=" && !compound_assign_ops[n.operator])
                    throw LowerNotSupported(`assignment operator ${n.operator}`, n.loc);
                if (n.left.type === b.Identifier) {
                    let binding = this.reference(n.left);
                    if (!binding) this.globalAssignedNames.add(n.left.name);
                } else this.walkExpr(n.left);
                this.walkExpr(n.right);
                return;
            case b.UpdateExpression:
                if (n.argument.type === b.Identifier) {
                    let binding = this.reference(n.argument);
                    if (!binding) this.globalAssignedNames.add(n.argument.name);
                } else if (n.argument.type === b.MemberExpression) {
                    this.walkExpr(n.argument);
                } else {
                    throw LowerNotSupported(`update of ${n.argument.type}`, n.loc);
                }
                return;
            case b.TemplateLiteral:
                for (let e of n.expressions) this.walkExpr(e);
                return;
            case b.CallExpression:
                // %-intrinsic calls (from the pre-EIR desugar passes):
                // the callee is a lowering directive, not a reference.
                // only whitelisted intrinsics lower; reject others early.
                if (n.callee.type === b.Identifier && n.callee.name[0] === "%") {
                    if (!eir_intrinsics[n.callee.name])
                        throw LowerNotSupported(`intrinsic ${n.callee.name}`, n.loc);
                    for (let a of n.arguments) this.walkExpr(a);
                    return;
                }
                if (n.callee.type === b.Identifier) this.reference(n.callee, true);
                else this.walkExpr(n.callee);
                for (let a of n.arguments) this.walkExpr(a);
                return;
            case b.NewExpression:
                this.walkExpr(n.callee);
                for (let a of n.arguments) this.walkExpr(a);
                return;
            case b.MemberExpression:
                this.walkExpr(n.object);
                if (n.computed) this.walkExpr(n.property);
                return;
            case b.ConditionalExpression:
                this.walkExpr(n.test);
                this.walkExpr(n.consequent);
                this.walkExpr(n.alternate);
                return;
            case b.FunctionExpression: {
                let name = (n.id && n.id.name) || `anon${this.anon_gen++}`;
                this.enterFunction(n, this.curFn ? `${this.curFn.name}.${name}` : name);
                this.walkFnBody(n.body);
                this.leaveFunction();
                return;
            }
            case b.ArrowFunctionExpression: {
                // arrows lower as ordinary closures, which is only correct
                // while they don't touch the lexical `this` (see the
                // ThisExpression case below)
                let name = `arrow${this.anon_gen++}`;
                this.enterFunction(n, this.curFn ? `${this.curFn.name}.${name}` : name);
                if (n.body.type === b.BlockStatement) this.walkFnBody(n.body);
                else this.walkExpr(n.body);
                this.leaveFunction();
                return;
            }
            case b.ThisExpression:
                if (this.curFn && this.curFn.node.type === b.ArrowFunctionExpression)
                    throw LowerNotSupported("lexical `this` in an arrow function", n.loc);
                return;
            case b.SequenceExpression:
                for (let e of n.expressions) this.walkExpr(e);
                return;
            case b.ArrayExpression:
                for (let e of n.elements) if (e) this.walkExpr(e);
                return;
            case b.ObjectExpression:
                for (let p of n.properties) {
                    if (p.computed) this.walkExpr(p.key);
                    this.walkExpr(p.value);
                }
                return;
            default:
                throw LowerNotSupported(`expression type ${n.type}`, n.loc);
        }
    }
}

// assign env slots for `info` and every function below it
function assignSlots(info) {
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
