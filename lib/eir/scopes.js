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
import { LowerNotSupported } from "./errors";

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
        this.curScope = null;
        this.curFn = null;
    }

    resolve(node) {
        return this.refs.get(node);
    }

    infoFor(fnNode) {
        return this.fnInfos.get(fnNode);
    }

    // --- entry point ---------------------------------------------------------

    analyzeFunction(fnNode, name) {
        let info = this.enterFunction(fnNode, name);
        this.walkStmt(fnNode.body);
        this.leaveFunction();
        assignSlots(info);
        return info;
    }

    enterFunction(fnNode, name) {
        let fname = name || (fnNode.id && fnNode.id.name) || "anon";
        let info = new FnInfo(fnNode, fname, this.curFn);
        this.fnInfos.set(fnNode, info);

        this.curFn = info;
        this.curScope = new LexScope(this.curScope, info);
        for (let p of fnNode.params) {
            if (p.type !== b.Identifier)
                throw new LowerNotSupported(`param pattern ${p.type}`, fnNode.loc);
            let binding = this.curScope.declare(p.name, "param");
            info.params.push(binding);
        }
        return info;
    }

    leaveFunction() {
        this.curScope = this.curScope.parent;
        this.curFn = this.curFn.parent;
    }

    reference(idNode) {
        if (idNode.name === "undefined") {
            this.refs.set(idNode, null);
            return;
        }
        let binding = this.curScope.lookup(idNode.name);
        this.refs.set(idNode, binding); // null = global
        if (!binding) return;

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
                        throw new LowerNotSupported(`declaration pattern ${d.id.type}`, n.loc);
                    if (d.init) this.walkExpr(d.init);
                    // declare after walking the init: `let x = x` refers outward
                    let binding = this.curScope.declare(d.id.name, "local");
                    this.refs.set(d.id, binding);
                }
                return;
            case b.FunctionDeclaration: {
                if (!n.id) throw new LowerNotSupported("unnamed function declaration", n.loc);
                let binding = this.curScope.declare(n.id.name, "fn");
                this.refs.set(n.id, binding);
                let name = this.curFn ? `${this.curFn.name}.${n.id.name}` : n.id.name;
                this.enterFunction(n, name);
                this.walkStmt(n.body);
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
                    if (n.init.type === b.VariableDeclaration) this.walkStmt(n.init);
                    else this.walkExpr(n.init);
                }
                if (n.test) this.walkExpr(n.test);
                if (n.update) this.walkExpr(n.update);
                this.walkStmt(n.body);
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
                    throw new LowerNotSupported("try/finally (desugar it first)", n.loc);
                if (!n.handlers || n.handlers.length !== 1)
                    throw new LowerNotSupported("try without exactly one catch", n.loc);
                this.walkStmt(n.block);
                let handler = n.handlers[0];
                this.curScope = new LexScope(this.curScope, this.curFn);
                if (handler.param) {
                    if (handler.param.type !== b.Identifier)
                        throw new LowerNotSupported("catch parameter pattern", n.loc);
                    let binding = this.curScope.declare(handler.param.name, "catch");
                    this.refs.set(handler.param, binding);
                }
                this.walkStmt(handler.body);
                this.curScope = this.curScope.parent;
                return;
            }
            case b.BreakStatement:
            case b.ContinueStatement:
                if (n.label) throw new LowerNotSupported("labeled break/continue", n.loc);
                return;
            case b.EmptyStatement:
                return;
            default:
                throw new LowerNotSupported(`statement type ${n.type}`, n.loc);
        }
    }

    // --- expressions ------------------------------------------------------------

    walkExpr(n) {
        switch (n.type) {
            case b.Literal:
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
                this.walkExpr(n.argument);
                return;
            case b.AssignmentExpression:
                if (n.left.type === b.Identifier) this.reference(n.left);
                else this.walkExpr(n.left);
                this.walkExpr(n.right);
                return;
            case b.CallExpression:
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
                let name = (n.id && n.id.name) || "anon";
                this.enterFunction(n, this.curFn ? `${this.curFn.name}.${name}` : name);
                this.walkStmt(n.body);
                this.leaveFunction();
                return;
            }
            case b.ThisExpression:
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
                throw new LowerNotSupported(`expression type ${n.type}`, n.loc);
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
