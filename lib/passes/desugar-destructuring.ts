/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

import { startGenerator, intrinsic } from "../echo-util";
import { TransformPass, VisitResult } from "../node-visitor";
import * as b from "../ast-builder";
import { reportError } from "../errors";
import {
    Symbol_id,
    iterator_id,
    createIteratorWrapper_id,
    getNextValue_id,
    getRest_id,
} from "../common-ids";
import type * as e from "../estree";

const gen = startGenerator();
const fresh = () => b.identifier(`%destruct_tmp${gen()}`);

// note: value-position identifiers must be fresh AST nodes per use (the
// EIR scope analysis resolves references in a node-keyed map), so the
// Symbol.iterator member expression is minted per call site
function symbolIterator(): e.MemberExpression {
    return b.memberExpression(b.identifier(Symbol_id.name), iterator_id);
}

// one desugared binding: `key` receives `value`.  need_decl bindings are
// synthesized temps; the rest are the pattern's own targets (declared in
// declaration position, assigned in assignment position).
interface Binding {
    key: e.Identifier | e.Pattern;
    value: e.Expression;
    need_decl?: boolean;
}

// bind `target` (an Identifier or a nested pattern) to `value`, applying
// the AssignmentPattern default `dflt` if present:
//   let %dt = value, target = %dt === undefined ? dflt : %dt;
function bindTarget(
    target: e.Pattern,
    value: e.Expression,
    dflt: e.Expression | null,
    bindings: Binding[]
): void {
    if (dflt) {
        const dt = fresh();
        bindings.push({ key: dt, value: value, need_decl: true });
        value = b.conditionalExpression(
            b.binaryExpression(b.identifier(dt.name), "===", b.undefinedLit()),
            dflt,
            b.identifier(dt.name)
        );
    }

    if (target.type === "Identifier") {
        bindings.push({ key: target, value: value });
        return;
    }

    // a nested pattern: land the (possibly defaulted) value in a temp and
    // recurse
    const pt = fresh();
    bindings.push({ key: pt, value: value, need_decl: true });
    if (target.type === "ObjectPattern")
        createObjectPatternBindings(b.identifier(pt.name), target, bindings);
    else if (target.type === "ArrayPattern")
        createArrayPatternBindingsUsingIterator(b.identifier(pt.name), target, bindings);
    else throw new Error(`bindTarget: target.type = ${target.type}`);
}

// given an assignment { pattern } = id
//
function createObjectPatternBindings(
    id: e.Identifier,
    pattern: e.ObjectPattern,
    bindings: Binding[]
): void {
    for (const prop of pattern.properties) {
        const memberexp = b.memberExpression(id, prop.key);
        if (prop.computed) memberexp.computed = true;

        let target = prop.value as e.Pattern;
        let dflt: e.Expression | null = null;
        if (target.type === "AssignmentPattern") {
            dflt = target.right;
            target = target.left;
        }

        bindTarget(target, memberexp, dflt, bindings);
    }
}

function createArrayPatternBindingsUsingIterator(
    id: e.Identifier,
    pattern: e.ArrayPattern,
    bindings: Binding[]
): void {
    let seen_spread = false;

    // first off we create an iterator and wrapper for the rhs
    const iter_id = fresh();
    const wrapper_id = fresh();
    bindings.push({
        key: iter_id,
        value: b.callExpression(b.memberExpression(id, symbolIterator(), true), []),
        need_decl: true,
    });
    bindings.push({
        key: wrapper_id,
        value: intrinsic(createIteratorWrapper_id, [iter_id]),
        need_decl: true,
    });

    const nextValue = () =>
        b.callExpression(b.memberExpression(b.identifier(wrapper_id.name), getNextValue_id), []);

    for (const el of pattern.elements) {
        if (seen_spread)
            reportError(
                SyntaxError,
                "elements after spread element in array pattern",
                "<unknown>",
                el && el.loc ? el.loc : undefined
            );

        if (el == null) {
            bindings.push({ key: fresh() /*unused*/, value: nextValue() });
        } else if (el.type === "SpreadElement" || el.type === "RestElement") {
            // declaration-position rests parse as SpreadElement,
            // assignment-position ones as RestElement
            bindings.push({
                key: el.argument as e.Pattern,
                value: b.callExpression(
                    b.memberExpression(b.identifier(wrapper_id.name), getRest_id),
                    []
                ),
            });
            seen_spread = true;
        } else {
            let target: e.Pattern = el;
            let dflt: e.Expression | null = null;
            if (target.type === "AssignmentPattern") {
                dflt = target.right;
                target = target.left;
            }
            bindTarget(target, nextValue(), dflt, bindings);
        }
    }
}

export class DesugarDestructuring extends TransformPass {
    // a pattern (or member-expression) loop head desugars to a fresh
    // identifier head plus a binding statement at the top of the body:
    //
    //   for (let [a, b] of xs) body   =>   for (let %t of xs) { let [a, b] = %t; body }
    //   for (o.x of xs) body          =>   for (let %t of xs) { o.x = %t; body }
    //
    // the inner statement then desugars through the ordinary
    // declaration/assignment paths.  body-scoped `let`s are fresh per
    // iteration, preserving per-iteration capture semantics.
    private desugarForHead(n: e.ForOfStatement | e.ForInStatement): VisitResult {
        const head = n.left;
        let bindStmt: VisitResult = null;
        if (head.type === "VariableDeclaration") {
            const d = head.declarations[0]!;
            if (head.declarations.length === 1 && d.id.type !== "Identifier") {
                const tmp = fresh();
                const inner = b.variableDeclaration(head.kind, d.id, b.identifier(tmp.name));
                bindStmt = this.visit(inner);
                const newHead = b.letDeclaration(tmp, null);
                // strip the placeholder init: a for-of/for-in head
                // declaration has no initializer
                newHead.declarations[0]!.init = null;
                n.left = newHead;
            }
        } else if (head.type !== "Identifier") {
            // ObjectPattern/ArrayPattern assignment form, or a member
            // expression target
            const tmp = fresh();
            const assign = b.expressionStatement(
                b.assignmentExpression(head, "=", b.identifier(tmp.name))
            );
            bindStmt = this.visit(assign);
            const newHead = b.letDeclaration(tmp, null);
            newHead.declarations[0]!.init = null;
            n.left = newHead;
        }
        n.right = this.visitAs(n.right);
        n.body = this.visitAs(n.body);
        if (bindStmt) {
            const stmts = (Array.isArray(bindStmt) ? bindStmt : [bindStmt]) as e.Statement[];
            n.body = b.blockStatement(stmts.concat([n.body]));
        }
        return n;
    }

    override visitForOf(n: e.ForOfStatement): VisitResult {
        return this.desugarForHead(n);
    }

    override visitForIn(n: e.ForInStatement): VisitResult {
        return this.desugarForHead(n);
    }

    // catch ({ message }) { ... }  =>  catch (%t) { let { message } = %t; ... }
    override visitCatchClause(n: e.CatchClause): VisitResult {
        if (n.param && n.param.type !== "Identifier") {
            const tmp = fresh();
            const bindDecl = this.visitAs<e.VariableDeclaration>(
                b.letDeclaration(n.param, b.identifier(tmp.name))
            );
            n.param = tmp;
            n.body = this.visitAs(n.body);
            n.body.body.unshift(bindDecl);
            return n;
        }
        return super.visitCatchClause(n);
    }

    override visitFunction(n: e.Function): VisitResult {
        // we visit the formal parameters directly, rewriting
        // them as tmp arg names and adding 'let' decls for the
        // pattern identifiers at the top of the function's
        // body.
        const new_params: e.Pattern[] = [];
        const new_decls: e.VariableDeclaration[] = [];
        for (const p of n.params) {
            if (p.type === "ObjectPattern" || p.type === "ArrayPattern") {
                const p_id = fresh();
                new_params.push(p_id);
                const bindings: Binding[] = [];
                if (p.type === "ObjectPattern") createObjectPatternBindings(p_id, p, bindings);
                else createArrayPatternBindingsUsingIterator(p_id, p, bindings);
                const new_decl = b.variableDeclaration(
                    "let",
                    bindings.map((binding) => b.variableDeclarator(binding.key, binding.value))
                );
                new_decls.push(new_decl);
            } else if (p.type === "Identifier") {
                // we just pass this along
                new_params.push(p);
            } else if (p.type === "RestElement" && p.argument.type === "Identifier") {
                // a trailing ...rest stays in place (EIR handles it natively)
                new_params.push(p);
            } else {
                throw new Error(
                    `unhandled type of formal parameter in DesugarDestructuring ${p.type}`
                );
            }
        }

        // expression-bodied arrows have no statement list: writing
        // n.body.body here used to clobber the body of `() => () => ...`
        // (the inner arrow's body field) with [undefined].  wrap in a
        // block only when there are decls to prepend.
        if (n.body.type === "BlockStatement") {
            n.body.body = (new_decls as e.Statement[]).concat(n.body.body);
        } else if (new_decls.length > 0) {
            n.body = b.blockStatement(
                (new_decls as e.Statement[]).concat([b.returnStatement(n.body)])
            );
            n.expression = false;
        }
        n.params = new_params;
        n.body = this.visitAs(n.body);
        return n;
    }

    override visitVariableDeclaration(n: e.VariableDeclaration): VisitResult {
        const decls: e.VariableDeclarator[] = [];

        for (const decl of n.declarations) {
            if (decl.id.type === "ObjectPattern" || decl.id.type === "ArrayPattern") {
                const tmp_id = fresh();
                const bindings: Binding[] = [];
                decls.push(b.variableDeclarator(tmp_id, this.visitNullable(decl.init ?? null)));
                if (decl.id.type === "ObjectPattern")
                    createObjectPatternBindings(tmp_id, decl.id, bindings);
                else createArrayPatternBindingsUsingIterator(tmp_id, decl.id, bindings);
                for (const binding of bindings) {
                    decls.push(b.variableDeclarator(binding.key, binding.value));
                }
            } else if (decl.id.type === "Identifier") {
                decl.init = this.visitNullable(decl.init ?? null);
                decls.push(decl);
            } else {
                reportError(
                    Error,
                    `unhandled type of variable declaration in DesugarDestructuring ${decl.id.type}`,
                    this.filename,
                    n.loc ?? undefined
                );
            }
        }
        n.declarations = decls;
        return n;
    }

    override visitAssignmentExpression(n: e.AssignmentExpression): VisitResult {
        if (n.left.type === "ObjectPattern" || n.left.type === "ArrayPattern") {
            if (n.operator !== "=")
                reportError(
                    Error,
                    "cannot use destructuring with assignment operators other than =",
                    this.filename,
                    n.loc ?? undefined
                );

            const obj_tmp_id = fresh();
            const tmp_decl = b.letDeclaration(obj_tmp_id, this.visitAs(n.right));

            const assignments: e.Statement[] = [];
            const bindings: Binding[] = [];
            if (n.left.type === "ObjectPattern")
                createObjectPatternBindings(obj_tmp_id, n.left, bindings);
            else createArrayPatternBindingsUsingIterator(obj_tmp_id, n.left, bindings);

            for (const binding of bindings) {
                if (binding.need_decl) {
                    assignments.push(b.letDeclaration(binding.key, binding.value));
                } else {
                    assignments.push(
                        b.expressionStatement(
                            b.assignmentExpression(binding.key, "=", binding.value)
                        )
                    );
                }
            }

            assignments.push(b.returnStatement(b.identifier(obj_tmp_id.name)));

            return b.callExpression(
                b.functionExpression(null, [], b.blockStatement([tmp_decl, ...assignments])),
                []
            );
        }
        return super.visitAssignmentExpression(n);
    }
}
