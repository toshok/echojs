/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

// AST -> EIR lowering, phase-2 skeleton.
//
// This intentionally covers only a whitelisted subset of the (desugared)
// AST; anything else throws LowerNotSupported so callers can fall back to
// the legacy LLVMIRVisitor per-function.  The subset grows until nothing
// falls back; see EIRProposal.md's migration plan.
//
// Handled today: literals, identifiers (params + lexical locals + global
// reads), let/var declarations, assignment (=), binary/logical/unary
// operators, member access, calls, if/else, while, return, blocks,
// expression statements.
//
// Deliberately NOT handled yet: closures/captured variables (env slots),
// try/catch (unwind edges), for-in, switch, module slots, `arguments`,
// construct, and everything else.

import * as b from "../ast-builder";
import { FunctionBuilder } from "./builder";
import { Module } from "./ir";

export class LowerNotSupported extends Error {
    constructor(what, loc) {
        let locstr = loc && loc.start ? ` at ${loc.start.line}:${loc.start.column}` : "";
        super(`EIR lowering does not support ${what}${locstr}`);
        this.what = what;
    }
}

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

export class LowerFunction {
    constructor(name, paramNames) {
        this.b = new FunctionBuilder(name, ["%this"].concat(paramNames));
        this.locals = new Set(paramNames);
    }

    // --- expressions ----------------------------------------------------------

    expr(n) {
        switch (n.type) {
            case b.Literal:
                return this.literal(n);
            case b.Identifier:
                return this.identifier(n);
            case b.BinaryExpression:
                return this.binary(n);
            case b.LogicalExpression:
                return this.logical(n);
            case b.UnaryExpression:
                return this.unary(n);
            case b.AssignmentExpression:
                return this.assignment(n);
            case b.CallExpression:
                return this.call(n);
            case b.MemberExpression:
                return this.member(n);
            case b.ConditionalExpression:
                return this.conditional(n);
            default:
                throw new LowerNotSupported(`expression type ${n.type}`, n.loc);
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
                throw new LowerNotSupported(`literal ${typeof n.value}`, n.loc);
        }
    }

    identifier(n) {
        if (n.name === "undefined") return this.b.constUndefined();
        if (this.locals.has(n.name)) return this.b.readVariable(n.name, this.b.cur);
        // not a local: a global reference
        return this.b.emit("get_global", [], { atom: n.name });
    }

    binary(n) {
        let op = binops[n.operator];
        if (!op) throw new LowerNotSupported(`binary operator ${n.operator}`, n.loc);
        let l = this.expr(n.left);
        let r = this.expr(n.right);
        return this.b.emit(op, [l, r], {});
    }

    logical(n) {
        // a && b / a || b: short-circuit via control flow; the joined value
        // is a block parameter.
        let l = this.expr(n.left);
        let lbool = this.b.emit("to_boolean", [l], {});

        let rhs_bb = this.b.newBlock("logical_rhs");
        let join_bb = this.b.newBlock("logical_join");
        let result = join_bb.addParam("logical");

        if (n.operator === "&&") this.b.condBr(lbool, rhs_bb, [], join_bb, [l]);
        else if (n.operator === "||") this.b.condBr(lbool, join_bb, [l], rhs_bb, []);
        else throw new LowerNotSupported(`logical operator ${n.operator}`, n.loc);
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
                throw new LowerNotSupported(`unary operator ${n.operator}`, n.loc);
        }
    }

    assignment(n) {
        if (n.operator !== "=")
            throw new LowerNotSupported(`assignment operator ${n.operator}`, n.loc);
        if (n.left.type === b.Identifier) {
            if (!this.locals.has(n.left.name))
                throw new LowerNotSupported(`assignment to non-local ${n.left.name}`, n.loc);
            let v = this.expr(n.right);
            this.b.writeVariable(n.left.name, this.b.cur, v);
            return v;
        }
        if (n.left.type === b.MemberExpression) {
            let obj = this.expr(n.left.object);
            let v;
            if (!n.left.computed && n.left.property.type === b.Identifier) {
                v = this.expr(n.right);
                this.b.emit("set_prop_atom", [obj, v], { atom: n.left.property.name });
            } else {
                let key = this.expr(n.left.property);
                v = this.expr(n.right);
                this.b.emit("set_prop", [obj, key, v], {});
            }
            return v;
        }
        throw new LowerNotSupported(`assignment target ${n.left.type}`, n.loc);
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
            // method call: `this` is the receiver
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
            callee = this.expr(n.callee);
            thisArg = this.b.constUndefined();
        }
        let args = n.arguments.map((a) => this.expr(a));
        return this.b.emit("call", [callee, thisArg].concat(args), {});
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
                        throw new LowerNotSupported(`declaration pattern ${d.id.type}`, n.loc);
                    let init = d.init ? this.expr(d.init) : this.b.constUndefined();
                    this.locals.add(d.id.name);
                    this.b.writeVariable(d.id.name, this.b.cur, init);
                }
                return;
            case b.ExpressionStatement:
                this.expr(n.expression);
                return;
            case b.IfStatement:
                return this.ifStmt(n);
            case b.WhileStatement:
                return this.whileStmt(n);
            case b.ReturnStatement:
                this.b.ret(n.argument ? this.expr(n.argument) : this.b.constUndefined());
                return;
            case b.EmptyStatement:
                return;
            default:
                throw new LowerNotSupported(`statement type ${n.type}`, n.loc);
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
        // header is NOT sealed yet: the back edge is still coming

        this.b.setInsertPoint(header);
        let cond = this.expr(n.test);
        let cbool = this.b.emit("to_boolean", [cond], {});
        this.b.condBr(cbool, body, [], exit, []);
        this.b.sealBlock(body);

        this.b.setInsertPoint(body);
        this.stmt(n.body);
        if (!this.b.cur.terminated) this.b.br(header, []);
        this.b.sealBlock(header);
        this.b.sealBlock(exit);

        this.b.setInsertPoint(exit);
    }

    finish() {
        if (!this.b.cur.terminated) this.b.ret(this.b.constUndefined());
        return this.b.finish();
    }
}

// lower a FunctionDeclaration/FunctionExpression AST node (no captures)
export function lowerFunctionNode(n, name) {
    let paramNames = n.params.map((p) => {
        if (p.type !== b.Identifier) throw new LowerNotSupported(`param pattern ${p.type}`, n.loc);
        return p.name;
    });
    let lf = new LowerFunction(name || (n.id && n.id.name) || "anon", paramNames);
    lf.stmt(n.body);
    return lf.finish();
}

// lower every top-level function declaration in a parsed program
export function lowerProgram(ast, moduleName) {
    let mod = new Module(moduleName || "module");
    for (let s of ast.body) {
        if (s.type === b.FunctionDeclaration) mod.addFunction(lowerFunctionNode(s));
    }
    return mod;
}
