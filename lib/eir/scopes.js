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
        if (parent) parent.children.push(this);
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
        // let/const loop variables: capturing one in a closure needs a
        // per-iteration environment, which lowering doesn't build; the
        // post-walk check in analyzeFunction falls back instead.
        this.loopLetBindings = [];
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
        for (let lb of this.loopLetBindings) {
            if (lb.captured)
                throw LowerNotSupported(
                    `closure capturing loop variable '${lb.name}' (needs per-iteration env)`,
                    fnNode.loc
                );
        }
        assignSlots(info);
        return info;
    }

    enterFunction(fnNode, name) {
        let fname = name || (fnNode.id && fnNode.id.name) || "anon";
        let info = new FnInfo(fnNode, fname, this.curFn);
        this.fnInfos.set(fnNode, info);

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

    reference(idNode, isCallee) {
        if (idNode.name === "undefined") {
            this.refs.set(idNode, null);
            return null;
        }
        if (idNode.name === "arguments")
            throw LowerNotSupported("the arguments object", idNode.loc);
        let binding = this.curScope.lookup(idNode.name);
        this.refs.set(idNode, binding); // null = global
        if (!binding) {
            this.globalNames.add(idNode.name);
            if (!isCallee) this.globalValueNames.add(idNode.name);
            return null;
        }

        if (binding.kind === "self") {
            // only direct recursion from the function itself is supported;
            // a nested function would need the closure value in its env,
            // and value-position uses would need the closure itself.
            if (binding.fnInfo !== this.curFn)
                throw LowerNotSupported("self-reference from a nested function", idNode.loc);
            if (!isCallee)
                throw LowerNotSupported("function self-reference as a value", idNode.loc);
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
            case b.VariableDeclaration:
                for (let d of n.declarations) {
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
                    if (d.init) this.walkExpr(d.init);
                }
                return;
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
                if (n.init) {
                    if (n.init.type === b.VariableDeclaration) {
                        this.walkStmt(n.init);
                        if (n.init.kind !== "var") {
                            for (let d of n.init.declarations) {
                                let binding = this.refs.get(d.id);
                                if (binding) this.loopLetBindings.push(binding);
                            }
                        }
                    } else this.walkExpr(n.init);
                }
                if (n.test) this.walkExpr(n.test);
                if (n.update) this.walkExpr(n.update);
                this.walkStmt(n.body);
                this.curScope = this.curScope.parent;
                return;
            }
            case b.ForInStatement:
            case b.ForOfStatement: {
                this.curScope = new LexScope(this.curScope, this.curFn);
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
                    if (n.left.kind !== "var") this.loopLetBindings.push(binding);
                } else if (n.left.type === b.Identifier) {
                    let binding = this.reference(n.left);
                    if (!binding) this.globalAssignedNames.add(n.left.name);
                } else {
                    throw LowerNotSupported(`for-of/for-in target ${n.left.type}`, n.loc);
                }
                this.walkExpr(n.right);
                this.walkStmt(n.body);
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
                if (n.finalizer)
                    throw LowerNotSupported("try/finally (desugar it first)", n.loc);
                if (!n.handlers || n.handlers.length !== 1)
                    throw LowerNotSupported("try without exactly one catch", n.loc);
                this.walkStmt(n.block);
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
    // allocates one; if it doesn't, its incoming env already *is* the parent
    let captured = info.bindings.filter((bd) => bd.captured);
    let wantsEnv = captured.length > 0;
    if (wantsEnv && info.needsParentEnv && info.parent !== null) {
        info.parentSlot = next++;
    }
    for (let bd of captured) bd.slot = next++;
    info.envSize = next;

    for (let child of info.children) assignSlots(child);
}
