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
// Handled: literals, identifiers (locals/captured/globals), var/let/const,
// assignment (= and compound), update (++/--), binary/logical/unary
// operators, member access, calls, new, this, sequence/array/object
// literals, untagged template literals, function declarations and
// expressions, arrow functions that don't use `this` (full closure
// support), default parameters, if/else, while, do-while, for, for-of,
// switch, break/continue, return, throw, try/catch (unwind edges).
//
// Not yet: for-in, `arguments`, rest params, tagged templates, regex
// literals, arrows using lexical `this`, closures over let/const loop
// variables (per-iteration envs), try/finally (desugar it first),
// labeled break/continue, getters/setters.

import * as b from "../ast-builder";
import { FunctionBuilder } from "./builder";
import { Module } from "./ir";
import { ScopeAnalysis, compound_assign_ops } from "./scopes";
import { LowerNotSupported, isLowerNotSupported } from "./errors";

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

class LowerFunction {
    constructor(info, analysis, module, mod_ctx) {
        this.info = info; // FnInfo from scope analysis
        this.analysis = analysis;
        this.module = module;
        // module-scope interop: module-slot references (imports and this
        // module's exports: name -> {module, slot, constval?, writable})
        // and sibling top-level EIR functions callable directly
        this.mod_ctx = mod_ctx || { refs: new Map(), siblings: new Map() };

        let paramNames = info.params.map((p) => p.uid);
        this.b = new FunctionBuilder(info.name, ["%env", "%this"].concat(paramNames));
        this.envParam = this.b.fn.entry.params[0];
        this.thisParam = this.b.fn.entry.params[1];

        // break/continue targets.  loops push onto both stacks; switch
        // statements only onto breakTargets (continue passes through a
        // switch to the enclosing loop).
        this.breakTargets = [];
        this.continueTargets = [];

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

        // default parameters: a param that arrived undefined takes its
        // default (evaluated left to right, in the function scope).  the
        // conditional write merges via SSA (or the env, for captured
        // params, whose initial store just happened above).
        let defaults = info.defaults || [];
        for (let i = 0; i < defaults.length; i++) {
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

    // the environment holding `binding`, from this function's point of view
    envForBinding(binding) {
        if (binding.fnInfo === this.info) return this.curEnv;
        // start from our incoming env (the environment current in our parent
        // when our closure was made) and follow parent slots upward
        let env = this.envParam;
        let a = this.nearestEnvAncestor(this.info);
        while (a && a !== binding.fnInfo) {
            if (a.parentSlot < 0)
                throw new Error(`EIR lowering: broken env chain through ${a.name}`);
            env = this.b.emit("env_load", [env], { slot: a.parentSlot });
            a = this.nearestEnvAncestor(a);
        }
        if (!a) throw new Error(`EIR lowering: env chain missed ${binding.uid}`);
        return env;
    }

    nearestEnvAncestor(f) {
        let p = f.parent;
        while (p && p.envSize === 0) p = p.parent;
        return p;
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
            case b.ThisExpression:
                return this.thisParam;
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
                let elems = n.elements.map((e) =>
                    e ? this.expr(e) : this.b.constUndefined()
                );
                return this.b.emit("make_array", elems, {});
            }
            case b.ObjectExpression: {
                let keys = [];
                let values = [];
                for (let p of n.properties) {
                    if (p.computed || (p.key.type !== b.Identifier && p.key.type !== b.Literal))
                        throw LowerNotSupported("computed object key", n.loc);
                    keys.push(p.key.type === b.Identifier ? p.key.name : String(p.key.value));
                    values.push(this.expr(p.value));
                }
                return this.b.emit("make_object", values, { keys: keys });
            }
            default:
                throw LowerNotSupported(`expression type ${n.type}`, n.loc);
        }
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
        return this.b.emit("make_closure", [this.curEnv], { fn: childInfo.name });
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
            default:
                throw LowerNotSupported(`unary operator ${n.operator}`, n.loc);
        }
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

    member(n) {
        let obj = this.expr(n.object);
        if (!n.computed && n.property.type === b.Identifier)
            return this.b.emit("get_prop_atom", [obj], { atom: n.property.name });
        let key = this.expr(n.property);
        return this.b.emit("get_prop", [obj, key], {});
    }

    call(n) {
        let callee, thisArg;
        if (n.callee.type === b.MemberExpression) {
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
                    if (d.id.type !== b.Identifier)
                        throw LowerNotSupported(`declaration pattern ${d.id.type}`, n.loc);
                    let init = d.init ? this.expr(d.init) : this.b.constUndefined();
                    let binding = this.analysis.resolve(d.id);
                    this.writeBinding(binding, init);
                }
                return;
            case b.FunctionDeclaration:
                // closure was created (hoisted) at entry; lower the body now
                lowerOneFunction(this.analysis.infoFor(n), this.analysis, this.module, this.mod_ctx);
                return;
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
            case b.SwitchStatement:
                return this.switchStmt(n);
            case b.ReturnStatement:
                this.b.ret(n.argument ? this.expr(n.argument) : this.b.constUndefined());
                return;
            case b.ThrowStatement:
                this.b.throwValue(this.expr(n.argument));
                return;
            case b.TryStatement:
                return this.tryStmt(n);
            case b.BreakStatement: {
                if (n.label || this.breakTargets.length === 0)
                    throw LowerNotSupported("break outside plain loop/switch", n.loc);
                this.b.br(this.breakTargets[this.breakTargets.length - 1], []);
                return;
            }
            case b.ContinueStatement: {
                if (n.label || this.continueTargets.length === 0)
                    throw LowerNotSupported("continue outside plain loop", n.loc);
                this.b.br(this.continueTargets[this.continueTargets.length - 1], []);
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
        this.b.setInsertPoint(body);
        this.stmt(n.body);
        if (!this.b.cur.terminated) this.b.br(header, []);
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
        this.b.setInsertPoint(body);
        this.stmt(n.body);
        if (!this.b.cur.terminated) this.b.br(cond_bb, []);
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
        this.b.setInsertPoint(body);
        this.stmt(n.body);
        if (!this.b.cur.terminated) this.b.br(update, []);
        this.breakTargets.pop();
        this.continueTargets.pop();
        this.b.sealBlock(update);

        this.b.setInsertPoint(update);
        if (n.update) this.expr(n.update);
        this.b.br(header, []);
        this.b.sealBlock(header);
        this.b.sealBlock(exit);
        this.b.setInsertPoint(exit);
    }

    // mirrors the legacy DesugarForOf expansion: iterable[Symbol.iterator]()
    // once, then `next()` per iteration, testing `.done` and binding `.value`
    forOfStmt(n) {
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
        let v = this.b.emit("get_prop_atom", [res], { atom: "value" });
        if (n.left.type === b.VariableDeclaration) {
            let binding = this.analysis.resolve(n.left.declarations[0].id);
            this.writeBinding(binding, v);
        } else {
            this.writeIdentifier(n.left, v);
        }
        this.breakTargets.push(exit);
        this.continueTargets.push(header);
        this.stmt(n.body);
        if (!this.b.cur.terminated) this.b.br(header, []);
        this.breakTargets.pop();
        this.continueTargets.pop();

        this.b.sealBlock(header);
        this.b.sealBlock(exit);
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

    tryStmt(n) {
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
