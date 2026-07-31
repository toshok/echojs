/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// The AST walker/transformer base.  visit() dispatches on node type to a
// per-type method; a method returning null/undefined keeps the original
// node, returning a node replaces it, and (inside statement/expression
// lists) returning an array splices.
//
// Transformers must preserve the syntactic category of the slot they
// return into (an expression position must get back an expression, ...).
// That contract is asserted in exactly one place — visitAs — rather than
// scattered casts; violations surface downstream in lowering, which
// whitelists what it understands.

import * as b from "./ast-builder";
import type * as e from "./estree";
import type { CompilerOptions } from "./options";

export type VisitResult = e.Node | e.Node[] | null | undefined;

export class TreeVisitor {
    // the category-preserving cast (see the module comment)
    protected visitAs<T extends e.Node>(n: T | null | undefined): T {
        return this.visit(n) as T;
    }

    protected visitNullable<T extends e.Node>(n: T | null): T | null {
        if (!n) return n;
        return this.visit(n) as T;
    }

    visitArrayKeep<T extends e.Node>(arr: (T | null)[]): (T | null)[] {
        return arr.map((el) => (el === null ? null : (this.visit(el) as T)));
    }

    // in-place transform of a node list: a falsy result removes the
    // element, an array result splices its elements in
    visitArray<T extends e.Node>(arr: T[]): T[] {
        let i = 0;
        let end = arr.length;

        while (i < end) {
            const tmp = this.visit(arr[i]) as T | T[] | null | undefined;
            if (!tmp) {
                arr.splice(i, 1);
                end = arr.length;
            } else if (Array.isArray(tmp)) {
                arr.splice(i, 1, ...tmp);
                i += tmp.length;
                end = arr.length;
            } else {
                arr[i] = tmp;
                i += 1;
            }
        }
        return arr;
    }

    visit(n: e.Node | e.Node[] | null | undefined): VisitResult {
        if (!n) return n;
        if (Array.isArray(n)) return this.visitArray(n);

        let rv: VisitResult = null;
        switch (n.type) {
            case "ArrayExpression":
                rv = this.visitArrayExpression(n);
                break;
            case "ArrayPattern":
                rv = this.visitArrayPattern(n);
                break;
            case "ArrowFunctionExpression":
                rv = this.visitArrowFunctionExpression(n);
                break;
            case "AssignmentExpression":
                rv = this.visitAssignmentExpression(n);
                break;
            case "AssignmentPattern":
                rv = this.visitAssignmentPattern(n);
                break;
            case "AwaitExpression":
                rv = this.visitAwaitExpression(n);
                break;
            case "BinaryExpression":
                rv = this.visitBinaryExpression(n);
                break;
            case "BlockStatement":
                rv = this.visitBlock(n);
                break;
            case "BreakStatement":
                rv = this.visitBreak(n);
                break;
            case "CallExpression":
                rv = this.visitCallExpression(n);
                break;
            case "CatchClause":
                rv = this.visitCatchClause(n);
                break;
            case "ChainExpression":
                rv = this.visitChainExpression(n);
                break;
            case "ClassBody":
                rv = this.visitClassBody(n);
                break;
            case "ClassDeclaration":
                rv = this.visitClassDeclaration(n);
                break;
            case "ClassExpression":
                rv = this.visitClassExpression(n);
                break;
            case "ConditionalExpression":
                rv = this.visitConditionalExpression(n);
                break;
            case "ContinueStatement":
                rv = this.visitContinue(n);
                break;
            case "DebuggerStatement":
                rv = n; // compiled as a no-op
                break;
            case "DoWhileStatement":
                rv = this.visitDo(n);
                break;
            case "EmptyStatement":
                rv = this.visitEmptyStatement(n);
                break;
            case "ExportNamedDeclaration":
                rv = this.visitExportNamedDeclaration(n);
                break;
            case "ExportAllDeclaration":
                rv = this.visitExportAllDeclaration(n);
                break;
            case "ExportDefaultDeclaration":
                rv = this.visitExportDefaultDeclaration(n);
                break;
            case "ExportSpecifier":
                rv = this.visitExportSpecifier(n);
                break;
            case "ExpressionStatement":
                rv = this.visitExpressionStatement(n);
                break;
            case "ForInStatement":
                rv = this.visitForIn(n);
                break;
            case "ForOfStatement":
                rv = this.visitForOf(n);
                break;
            case "ForStatement":
                rv = this.visitFor(n);
                break;
            case "FunctionDeclaration":
                rv = this.visitFunctionDeclaration(n);
                break;
            case "FunctionExpression":
                rv = this.visitFunctionExpression(n);
                break;
            case "Identifier":
                rv = this.visitIdentifier(n);
                break;
            case "IfStatement":
                rv = this.visitIf(n);
                break;
            case "ImportDeclaration":
                rv = this.visitImportDeclaration(n);
                break;
            case "ImportSpecifier":
                rv = this.visitImportSpecifier(n);
                break;
            case "ImportDefaultSpecifier":
                rv = this.visitImportDefaultSpecifier(n);
                break;
            case "ImportNamespaceSpecifier":
                rv = this.visitImportNamespaceSpecifier(n);
                break;
            case "LabeledStatement":
                rv = this.visitLabeledStatement(n);
                break;
            case "Literal":
                rv = this.visitLiteral(n);
                break;
            case "LogicalExpression":
                rv = this.visitLogicalExpression(n);
                break;
            case "MemberExpression":
                rv = this.visitMemberExpression(n);
                break;
            case "MetaProperty":
                rv = this.visitMetaProperty(n);
                break;
            case "MethodDefinition":
                rv = this.visitMethodDefinition(n);
                break;
            case "NewExpression":
                rv = this.visitNewExpression(n);
                break;
            case "ObjectExpression":
                rv = this.visitObjectExpression(n);
                break;
            case "ObjectPattern":
                rv = this.visitObjectPattern(n);
                break;
            case "PrivateIdentifier":
                rv = this.visitPrivateIdentifier(n);
                break;
            case "Program":
                rv = this.visitProgram(n);
                break;
            case "PropertyDefinition":
                rv = this.visitPropertyDefinition(n);
                break;
            case "Property":
                rv = this.visitProperty(n);
                break;
            case "RestElement":
                rv = this.visitRestElement(n);
                break;
            case "ReturnStatement":
                rv = this.visitReturn(n);
                break;
            case "SequenceExpression":
                rv = this.visitSequenceExpression(n);
                break;
            case "SpreadElement":
                rv = this.visitSpreadElement(n);
                break;
            case "StaticBlock":
                rv = this.visitStaticBlock(n);
                break;
            case "Super":
                rv = this.visitSuper(n);
                break;
            case "SwitchCase":
                rv = this.visitCase(n);
                break;
            case "SwitchStatement":
                rv = this.visitSwitch(n);
                break;
            case "TaggedTemplateExpression":
                rv = this.visitTaggedTemplateExpression(n);
                break;
            case "TemplateElement":
                rv = this.visitTemplateElement(n);
                break;
            case "TemplateLiteral":
                rv = this.visitTemplateLiteral(n);
                break;
            case "ThisExpression":
                rv = this.visitThisExpression(n);
                break;
            case "ThrowStatement":
                rv = this.visitThrow(n);
                break;
            case "TryStatement":
                rv = this.visitTry(n);
                break;
            case "UnaryExpression":
                rv = this.visitUnaryExpression(n);
                break;
            case "UpdateExpression":
                rv = this.visitUpdateExpression(n);
                break;
            case "VariableDeclaration":
                rv = this.visitVariableDeclaration(n);
                break;
            case "VariableDeclarator":
                rv = this.visitVariableDeclarator(n);
                break;
            case "WhileStatement":
                rv = this.visitWhile(n);
                break;
            case "WithStatement":
                rv = this.visitWith(n);
                break;
            case "YieldExpression":
                rv = this.visitYield(n);
                break;
            default:
                throw new Error(
                    `PANIC: unknown parse node type ${(n as e.Node).type}, ${JSON.stringify(n)}`
                );
        }

        if (rv == null) return n;
        return rv;
    }

    visitProgram(n: e.Program): VisitResult {
        n.body = this.visitArray(n.body);
        return n;
    }

    visitFunction(n: e.Function): VisitResult {
        n.params = this.visitArray(n.params);
        n.body = this.visitAs(n.body);
        return n;
    }

    visitFunctionDeclaration(n: e.FunctionDeclaration): VisitResult {
        return this.visitFunction(n);
    }

    visitFunctionExpression(n: e.FunctionExpression): VisitResult {
        return this.visitFunction(n);
    }

    visitArrowFunctionExpression(n: e.ArrowFunctionExpression): VisitResult {
        return this.visitFunction(n);
    }

    visitBlock(n: e.BlockStatement): VisitResult {
        n.body = this.visitArray(n.body);
        return n;
    }

    visitEmptyStatement(n: e.EmptyStatement): VisitResult {
        return n;
    }

    visitExpressionStatement(n: e.ExpressionStatement): VisitResult {
        n.expression = this.visitAs(n.expression);
        return n;
    }

    visitSwitch(n: e.SwitchStatement): VisitResult {
        n.discriminant = this.visitAs(n.discriminant);
        n.cases = this.visitArray(n.cases);
        return n;
    }

    visitCase(n: e.SwitchCase): VisitResult {
        n.test = this.visitNullable(n.test);
        n.consequent = this.visitArray(n.consequent);
        return n;
    }

    visitFor(n: e.ForStatement): VisitResult {
        n.init = this.visitNullable(n.init);
        n.test = this.visitNullable(n.test);
        n.update = this.visitNullable(n.update);
        n.body = this.visitAs(n.body);
        return n;
    }

    visitWhile(n: e.WhileStatement): VisitResult {
        n.test = this.visitAs(n.test);
        n.body = this.visitAs(n.body);
        return n;
    }

    visitIf(n: e.IfStatement): VisitResult {
        n.test = this.visitAs(n.test);
        n.consequent = this.visitAs(n.consequent);
        n.alternate = this.visitNullable(n.alternate);
        return n;
    }

    visitForIn(n: e.ForInStatement): VisitResult {
        n.left = this.visitAs(n.left);
        n.right = this.visitAs(n.right);
        n.body = this.visitAs(n.body);
        return n;
    }

    visitForOf(n: e.ForOfStatement): VisitResult {
        n.left = this.visitAs(n.left);
        n.right = this.visitAs(n.right);
        n.body = this.visitAs(n.body);
        return n;
    }

    visitDo(n: e.DoWhileStatement): VisitResult {
        n.body = this.visitAs(n.body);
        n.test = this.visitAs(n.test);
        return n;
    }

    visitIdentifier(n: e.Identifier): VisitResult {
        return n;
    }

    visitLiteral(n: e.Literal): VisitResult {
        return n;
    }

    visitThisExpression(n: e.ThisExpression): VisitResult {
        return n;
    }

    visitBreak(n: e.BreakStatement): VisitResult {
        return n;
    }

    visitContinue(n: e.ContinueStatement): VisitResult {
        return n;
    }

    visitTry(n: e.TryStatement): VisitResult {
        n.block = this.visitAs(n.block);
        if (n.handlers) n.handlers = this.visitArray(n.handlers);
        n.finalizer = this.visitNullable(n.finalizer);
        return n;
    }

    visitCatchClause(n: e.CatchClause): VisitResult {
        n.param = this.visitAs(n.param);
        n.guard = this.visitNullable(n.guard);
        n.body = this.visitAs(n.body);
        return n;
    }

    visitThrow(n: e.ThrowStatement): VisitResult {
        n.argument = this.visitAs(n.argument);
        return n;
    }

    visitRestElement(n: e.RestElement): VisitResult {
        return n;
    }

    visitReturn(n: e.ReturnStatement): VisitResult {
        n.argument = this.visitNullable(n.argument);
        return n;
    }

    visitWith(n: e.WithStatement): VisitResult {
        n.object = this.visitAs(n.object);
        n.body = this.visitAs(n.body);
        return n;
    }

    visitYield(n: e.YieldExpression): VisitResult {
        n.argument = this.visitNullable(n.argument);
        return n;
    }

    visitAwaitExpression(n: e.AwaitExpression): VisitResult {
        n.argument = this.visitAs(n.argument);
        return n;
    }

    visitVariableDeclaration(n: e.VariableDeclaration): VisitResult {
        n.declarations = this.visitArray(n.declarations);
        return n;
    }

    visitVariableDeclarator(n: e.VariableDeclarator): VisitResult {
        n.id = this.visitAs(n.id);
        if (n.init) n.init = this.visitAs(n.init);
        return n;
    }

    visitLabeledStatement(n: e.LabeledStatement): VisitResult {
        n.label = this.visitAs(n.label);
        n.body = this.visitAs(n.body);
        return n;
    }

    visitAssignmentExpression(n: e.AssignmentExpression): VisitResult {
        n.left = this.visitAs(n.left);
        n.right = this.visitAs(n.right);
        return n;
    }

    visitConditionalExpression(n: e.ConditionalExpression): VisitResult {
        n.test = this.visitAs(n.test);
        n.consequent = this.visitAs(n.consequent);
        n.alternate = this.visitAs(n.alternate);
        return n;
    }

    visitLogicalExpression(n: e.LogicalExpression): VisitResult {
        n.left = this.visitAs(n.left);
        n.right = this.visitAs(n.right);
        return n;
    }

    visitBinaryExpression(n: e.BinaryExpression): VisitResult {
        n.left = this.visitAs(n.left);
        n.right = this.visitAs(n.right);
        return n;
    }

    visitUnaryExpression(n: e.UnaryExpression): VisitResult {
        n.argument = this.visitAs(n.argument);
        return n;
    }

    visitUpdateExpression(n: e.UpdateExpression): VisitResult {
        n.argument = this.visitAs(n.argument);
        return n;
    }

    visitMemberExpression(n: e.MemberExpression): VisitResult {
        n.object = this.visitAs(n.object);
        if (n.computed) n.property = this.visitAs(n.property);
        return n;
    }

    visitSequenceExpression(n: e.SequenceExpression): VisitResult {
        n.expressions = this.visitArray(n.expressions);
        return n;
    }

    visitSuper(n: e.Super): VisitResult {
        return n;
    }

    visitSpreadElement(n: e.SpreadElement): VisitResult {
        n.argument = this.visitAs(n.argument);
        return n;
    }

    visitNewExpression(n: e.NewExpression): VisitResult {
        n.callee = this.visitAs(n.callee);
        n.arguments = this.visitArray(n.arguments);
        return n;
    }

    visitObjectExpression(n: e.ObjectExpression): VisitResult {
        n.properties = this.visitArray(n.properties);
        return n;
    }

    visitArrayExpression(n: e.ArrayExpression): VisitResult {
        // esprima encodes holes in the array as 'null' elements in
        // n.elements, so we can't use visitArray.  instead iterate
        // over the elements manually.
        n.elements = this.visitArrayKeep(n.elements);
        return n;
    }

    visitProperty(n: e.Property): VisitResult {
        n.key = this.visitAs(n.key);
        n.value = this.visitAs(n.value);
        return n;
    }

    visitCallExpression(n: e.CallExpression): VisitResult {
        n.callee = this.visitAs(n.callee);
        n.arguments = this.visitArray(n.arguments);
        return n;
    }

    visitChainExpression(n: e.ChainExpression): VisitResult {
        n.expression = this.visitAs(n.expression);
        return n;
    }

    visitClassDeclaration(n: e.ClassDeclaration): VisitResult {
        return this.visitClass(n);
    }

    visitClassExpression(n: e.ClassExpression): VisitResult {
        return this.visitClass(n);
    }

    visitClass(n: e.Class): VisitResult {
        n.body = this.visitAs(n.body);
        return n;
    }

    visitClassBody(n: e.ClassBody): VisitResult {
        n.body = this.visitArray(n.body);
        return n;
    }

    visitMetaProperty(n: e.MetaProperty): VisitResult {
        return n;
    }

    visitMethodDefinition(n: e.MethodDefinition): VisitResult {
        n.value = this.visitAs(n.value);
        return n;
    }

    visitPropertyDefinition(n: e.PropertyDefinition): VisitResult {
        if (n.computed) n.key = this.visitAs(n.key);
        n.value = this.visitNullable(n.value);
        return n;
    }

    visitStaticBlock(n: e.StaticBlock): VisitResult {
        n.body = this.visitArray(n.body);
        return n;
    }

    visitPrivateIdentifier(n: e.PrivateIdentifier): VisitResult {
        return n;
    }

    visitExportDefaultDeclaration(n: e.ExportDefaultDeclaration): VisitResult {
        n.declaration = this.visitAs(n.declaration);
        return n;
    }

    visitExportNamedDeclaration(n: e.ExportNamedDeclaration): VisitResult {
        n.declaration = this.visitNullable(n.declaration);
        // XXX specifiers?
        return n;
    }

    visitExportAllDeclaration(n: e.ExportAllDeclaration): VisitResult {
        return n;
    }

    visitExportSpecifier(n: e.ExportSpecifier): VisitResult {
        return n;
    }

    visitImportDeclaration(n: e.ImportDeclaration): VisitResult {
        n.specifiers = this.visitArray(n.specifiers);
        return n;
    }

    visitImportSpecifier(n: e.ImportSpecifier): VisitResult {
        n.imported = this.visitAs(n.imported);
        return n;
    }

    visitImportDefaultSpecifier(n: e.ImportDefaultSpecifier): VisitResult {
        return n;
    }

    visitImportNamespaceSpecifier(n: e.ImportNamespaceSpecifier): VisitResult {
        return n;
    }

    visitArrayPattern(n: e.ArrayPattern): VisitResult {
        n.elements = this.visitArrayKeep(n.elements);
        return n;
    }

    visitAssignmentPattern(n: e.AssignmentPattern): VisitResult {
        // the left side is a binding pattern, not a reference
        n.right = this.visitAs(n.right);
        return n;
    }

    visitObjectPattern(n: e.ObjectPattern): VisitResult {
        n.properties = this.visitArray(n.properties);
        return n;
    }

    visitTaggedTemplateExpression(n: e.TaggedTemplateExpression): VisitResult {
        n.quasi = this.visitAs(n.quasi);
        return n;
    }

    visitTemplateLiteral(n: e.TemplateLiteral): VisitResult {
        n.quasis = this.visitArray(n.quasis);
        n.expressions = this.visitArray(n.expressions);
        return n;
    }

    visitTemplateElement(n: e.TemplateElement): VisitResult {
        return n;
    }

    toString(): string {
        return "TreeVisitor";
    }
}

export class TransformPass extends TreeVisitor {
    options: CompilerOptions;
    filename: string;

    constructor(options: CompilerOptions, filename?: string) {
        super();
        this.options = options;
        this.filename = filename ?? "<unknown>";
    }
}
