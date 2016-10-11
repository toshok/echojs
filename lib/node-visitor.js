/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import * as b from './ast-builder';

export class TreeVisitor {
    visitArrayKeep (arr, ...args) {
        return arr.map((el) => this.visit(el, ...args));
    }

    visitArray (arr, ...args) {
        let i = 0;
        let e = arr.length;
        
        while (i < e) {
            let tmp = this.visit(arr[i], ...args);
            if (!tmp) {
                arr.splice(i, 1);
                e = arr.length;
            }
            else if (Array.isArray(tmp)) {
                let tmplen = tmp.length;
                if (tmplen > 0) {
                    tmp.unshift(1);
                    tmp.unshift(i);
                    arr.splice.apply(arr, tmp);
                    i += tmplen;
                    e = arr.length;
                }
                else {
                    arr.splice(i, 1);
                    e = arr.length;
                }
            }
            else {
                arr[i] = tmp;
                i += 1;
            }
        }
        return arr;
    }
    
    
    visit (n, ...args) {
        if (!n) return n;
        if (Array.isArray(n)) return this.visitArray(n, ...args);

        let rv = null;
        switch (n.type) {
        case b.ArrayExpression:              rv = this.visitArrayExpression(n, ...args);         break;
        case b.ArrayPattern:                 rv = this.visitArrayPattern(n, ...args);            break;
        case b.ArrowFunctionExpression:      rv = this.visitArrowFunctionExpression(n, ...args); break;
        case b.AssignmentExpression:         rv = this.visitAssignmentExpression(n, ...args);    break;
        case b.BinaryExpression:             rv = this.visitBinaryExpression(n, ...args);        break;
        case b.BlockStatement:               rv = this.visitBlock(n, ...args);                   break;
        case b.BreakStatement:               rv = this.visitBreak(n, ...args);                   break;
        case b.CallExpression:               rv = this.visitCallExpression(n, ...args);          break;
        case b.CatchClause:                  rv = this.visitCatchClause(n, ...args);             break;
        case b.ClassBody:                    rv = this.visitClassBody(n, ...args);               break;
        case b.ClassDeclaration:             rv = this.visitClassDeclaration(n, ...args);        break;
        case b.ClassExpression:              rv = this.visitClassExpression(n, ...args);         break;
        case b.ClassHeritage:                throw new Error(`Unhandled AST node type: ${n.type}, ${JSON.stringify(n)}`);
        case b.ComprehensionBlock:           throw new Error(`Unhandled AST node type: ${n.type}, ${JSON.stringify(n)}`);
        case b.ComprehensionExpression:      throw new Error(`Unhandled AST node type: ${n.type}, ${JSON.stringify(n)}`);
        case b.ConditionalExpression:        rv = this.visitConditionalExpression(n, ...args); break;
        case b.ContinueStatement:            rv = this.visitContinue(n, ...args); break;
        case b.DebuggerStatement:            throw new Error(`Unhandled AST node type: ${n.type}, ${JSON.stringify(n)}`);
        case b.DoWhileStatement:             rv = this.visitDo(n, ...args); break;
        case b.EmptyStatement:               rv = this.visitEmptyStatement(n, ...args); break;
        case b.ExportNamedDeclaration:       rv = this.visitExportNamedDeclaration(n, ...args); break; // XXX jquery esprima
        case b.ExportAllDeclaration:         rv = this.visitExportAllDeclaration(n, ...args); break; // XXX jquery esprima
        case b.ExportDefaultDeclaration:     rv = this.visitExportDefaultDeclaration(n, ...args); break; // XXX jquery esprima
        case b.ExpressionStatement:          rv = this.visitExpressionStatement(n, ...args); break;
        case b.ForInStatement:               rv = this.visitForIn(n, ...args); break;
        case b.ForOfStatement:               rv = this.visitForOf(n, ...args); break;
        case b.ForStatement:                 rv = this.visitFor(n, ...args); break;
        case b.FunctionDeclaration:          rv = this.visitFunctionDeclaration(n, ...args); break;
        case b.FunctionExpression:           rv = this.visitFunctionExpression(n, ...args); break;
        case b.Identifier:                   rv = this.visitIdentifier(n, ...args); break;
        case b.IfStatement:                  rv = this.visitIf(n, ...args); break;
        case b.ImportDeclaration:            rv = this.visitImportDeclaration(n, ...args); break;
        case b.ImportSpecifier:              rv = this.visitImportSpecifier(n, ...args); break;
        case b.LabeledStatement:             rv = this.visitLabeledStatement(n, ...args); break;
        case b.Literal:                      rv = this.visitLiteral(n, ...args); break;
        case b.LogicalExpression:            rv = this.visitLogicalExpression(n, ...args); break;
        case b.MemberExpression:             rv = this.visitMemberExpression(n, ...args); break;
        case b.MetaProperty:                 rv = this.visitMetaProperty(n, ...args); break;
        case b.MethodDefinition:             rv = this.visitMethodDefinition(n, ...args); break;
        case b.ModuleDeclaration:            rv = this.visitModuleDeclaration(n, ...args); break;
        case b.NewExpression:                rv = this.visitNewExpression(n, ...args); break;
        case b.ObjectExpression:             rv = this.visitObjectExpression(n, ...args); break;
        case b.ObjectPattern:                rv = this.visitObjectPattern(n, ...args); break;
        case b.Program:                      rv = this.visitProgram(n, ...args); break;
        case b.Property:                     rv = this.visitProperty(n, ...args); break;
        case b.RestElement:                  rv = this.visitRestElement(n, ...args); break;
        case b.ReturnStatement:              rv = this.visitReturn(n, ...args); break;
        case b.SequenceExpression:           rv = this.visitSequenceExpression(n, ...args); break;
        case b.SpreadElement:                rv = this.visitSpreadElement(n, ...args); break;
        case b.Super:                        rv = this.visitSuper(n, ...args); break;
        case b.SwitchCase:                   rv = this.visitCase(n, ...args); break;
        case b.SwitchStatement:              rv = this.visitSwitch(n, ...args); break;
        case b.TaggedTemplateExpression:     rv = this.visitTaggedTemplateExpression(n, ...args); break;
        case b.TemplateElement:              rv = this.visitTemplateElement(n, ...args); break;
        case b.TemplateLiteral:              rv = this.visitTemplateLiteral(n, ...args); break;
        case b.ThisExpression:               rv = this.visitThisExpression(n, ...args); break;
        case b.ThrowStatement:               rv = this.visitThrow(n, ...args); break;
        case b.TryStatement:                 rv = this.visitTry(n, ...args); break;
        case b.UnaryExpression:              rv = this.visitUnaryExpression(n, ...args); break;
        case b.UpdateExpression:             rv = this.visitUpdateExpression(n, ...args); break;
        case b.VariableDeclaration:          rv = this.visitVariableDeclaration(n, ...args); break;
        case b.VariableDeclarator:           rv = this.visitVariableDeclarator(n, ...args); break;
        case b.WhileStatement:               rv = this.visitWhile(n, ...args); break;
        case b.WithStatement:                rv = this.visitWith(n, ...args); break;
        case b.YieldExpression:              rv = this.visitYield(n, ...args); break;
        default:
            throw new Error(`PANIC: unknown parse node type ${n.type}, ${JSON.stringify(n)}`);
        }
        
        if (rv == null) return n;
        return rv;
    }

    visitProgram (n, ...args) {
        n.body = this.visitArray(n.body, ...args);
        return n;
    }
    
    visitFunction (n, ...args) {
        n.params = this.visitArray(n.params, ...args);
        n.body   = this.visit(n.body, ...args);
        return n;
    }

    visitFunctionDeclaration (n, ...args) {
        return this.visitFunction(n, ...args);
    }

    visitFunctionExpression (n, ...args) {
        return this.visitFunction(n, ...args);
    }

    visitArrowFunctionExpression (n, ...args) {
        return this.visitFunction(n, ...args);
    }

    visitBlock (n, ...args) {
        n.body = this.visitArray(n.body, ...args);
        return n;
    }

    visitEmptyStatement (n) { return n; }

    visitExpressionStatement (n, ...args) {
        n.expression = this.visit(n.expression, ...args);
        return n;
    }
    
    visitSwitch (n, ...args) {
        n.discriminant = this.visit(n.discriminant, ...args);
        n.cases        = this.visitArray(n.cases, ...args);
        return n;
    }
    
    visitCase (n, ...args) {
        n.test       = this.visit(n.test, ...args);
        n.consequent = this.visit(n.consequent, ...args);
        return n;
    }
    
    visitFor (n, ...args) {
        n.init   = this.visit(n.init, ...args);
        n.test   = this.visit(n.test, ...args);
        n.update = this.visit(n.update, ...args);
        n.body   = this.visit(n.body, ...args);
        return n;
    }
    
    visitWhile (n, ...args) {
        n.test = this.visit(n.test, ...args);
        n.body = this.visit(n.body, ...args);
        return n;
    }
    
    visitIf (n, ...args) {
        n.test       = this.visit(n.test, ...args);
        n.consequent = this.visit(n.consequent, ...args);
        n.alternate  = this.visit(n.alternate, ...args);
        return n;
    }
    
    visitForIn (n, ...args) {
        n.left  = this.visit(n.left, ...args);
        n.right = this.visit(n.right, ...args);
        n.body  = this.visit(n.body, ...args);
        return n;
    }
    
    visitForOf (n, ...args) {
        n.left  = this.visit(n.left, ...args);
        n.right = this.visit(n.right, ...args);
        n.body  = this.visit(n.body, ...args);
        return n;
    }
    
    visitDo (n, ...args) {
        n.body = this.visit(n.body, ...args);
        n.test = this.visit(n.test, ...args);
        return n;
    }
    
    visitIdentifier (n)     { return n; }
    visitLiteral (n)        { return n; }
    visitThisExpression (n) { return n; }
    visitBreak (n)          { return n; }
    visitContinue (n)       { return n; }
    
    visitTry (n, ...args) {
        n.block = this.visit(n.block, ...args);
        if (n.handlers)
            n.handlers = this.visit(n.handlers, ...args);
        else
            n.handlers = null;
        n.finalizer = this.visit(n.finalizer, ...args);
        return n;
    }

    visitCatchClause (n, ...args) {
        n.param = this.visit(n.param, ...args);
        n.guard = this.visit(n.guard, ...args);
        n.body  = this.visit(n.body, ...args);
        return n;
    }
    
    visitThrow (n, ...args) {
        n.argument = this.visit(n.argument, ...args);
        return n;
    }

    visitRestElement (n) {
        return n;
    }

    visitReturn (n, ...args) {
        n.argument = this.visit(n.argument, ...args);
        return n;
    }
    
    visitWith (n, ...args) {
        n.object = this.visit(n.object, ...args);
        n.body   = this.visit(n.body, ...args);
        return n;
    }

    visitYield (n, ...args) {
        n.argument = this.visit(n.argument, ...args);
        return n;
    }
    
    visitVariableDeclaration (n, ...args) {
        n.declarations = this.visitArray(n.declarations, ...args);
        return n;
    }

    visitVariableDeclarator (n, ...args) {
        n.id   = this.visit(n.id, ...args);
        n.init = this.visit(n.init, ...args);
        return n;
    }
    
    visitLabeledStatement (n, ...args) {
        n.label = this.visit(n.label, ...args);
        n.body  = this.visit(n.body, ...args);
        return n;
    }
    
    visitAssignmentExpression (n, ...args) {
        n.left  = this.visit(n.left, ...args);
        n.right = this.visit(n.right, ...args);
        return n;
    }
    
    visitConditionalExpression (n, ...args) {
        n.test       = this.visit(n.test, ...args);
        n.consequent = this.visit(n.consequent, ...args);
        n.alternate  = this.visit(n.alternate, ...args);
        return n;
    }
    
    visitLogicalExpression (n, ...args) {
        n.left  = this.visit(n.left, ...args);
        n.right = this.visit(n.right, ...args);
        return n;
    }
    
    visitBinaryExpression (n, ...args) {
        n.left  = this.visit(n.left, ...args);
        n.right = this.visit(n.right, ...args);
        return n;
    }

    visitUnaryExpression (n, ...args) {
        n.argument = this.visit(n.argument, ...args);
        return n;
    }

    visitUpdateExpression (n, ...args) {
        n.argument = this.visit(n.argument, ...args);
        return n;
    }

    visitMemberExpression (n, ...args) {
        n.object = this.visit(n.object, ...args);
        if (n.computed)
            n.property = this.visit(n.property, ...args);
        return n;
    }
    
    visitSequenceExpression (n, ...args) {
        n.expressions = this.visitArray(n.expressions, ...args);
        return n;
    }

    visitSuper (n) {
        return n;
    }

    visitSpreadElement (n, ...args) {
        n.arguments = this.visit(n.argument, ...args);
        return n;
    }

    visitNewExpression (n, ...args) {
        n.callee    = this.visit(n.callee, ...args);
        n.arguments = this.visitArray(n.arguments, ...args);
        return n;
    }

    visitObjectExpression (n, ...args) {
        n.properties = this.visitArray(n.properties, ...args);
        return n;
    }

    visitArrayExpression (n, ...args) {
        // esprima encodes holes in the array as 'null' elements in
        // n.elements, so we can't use visitArray.  instead iterate
        // over the elements manually.
        n.elements = this.visitArrayKeep(n.elements, ...args);
        return n;
    }

    visitProperty (n, ...args) {
        n.key   = this.visit(n.key, ...args);
        n.value = this.visit(n.value, ...args);
        return n;
    }
    
    visitCallExpression (n, ...args) {
        n.callee    = this.visit(n.callee, ...args);
        n.arguments = this.visitArray(n.arguments, ...args);
        return n;
    }

    visitClassDeclaration (n, ...args) {
        return this.visitClass(n, ...args);
    }

    visitClassExpression (n, ...args) {
        return this.visitClass(n, ...args);
    }

    visitClass (n, ...args) {
        n.body = this.visit(n.body, ...args);
        return n;
    }

    visitClassBody (n, ...args) {
        n.body = this.visitArray(n.body, ...args);
        return n;
    }

    visitMetaProperty (n) {
        return n;
    }

    visitMethodDefinition (n, ...args) {
        n.value = this.visit(n.value, ...args);
        return n;
    }

    visitModuleDeclaration (n, ...args) {
        n.id   = this.visit(n.id, ...args);
        n.body = this.visit(n.body, ...args);
        return n;
    }

    visitExportDefaultDeclaration (n, ...args) {
        n.declaration = this.visit(n.declaration, ...args);
        return n;
    }

    visitExportNamedDeclaration (n, ...args) {
        n.declaration = this.visit(n.declaration, ...args);
        // XXX specifiers?
        return n;
    }

    visitExportAllDeclaration (n) {
        return n;
    }
    
    visitImportDeclaration (n, ...args) {
        n.specifiers = this.visitArray(n.specifiers, ...args);
        return n;
    }

    visitImportSpecifier (n, ...args) {
        n.imported = this.visit(n.imported, ...args);
        return n;
    }

    visitArrayPattern (n, ...args) {
        n.elements = this.visitArrayKeep(n.elements, ...args);
        return n;
    }

    visitObjectPattern (n, ...args) {
        n.properties = this.visitArray(n.properties, ...args);
        return n;
    }

    visitTaggedTemplateExpression (n, ...args) {
        n.quasi = this.visit(n.quasi, ...args);
        return n;
    }

    visitTemplateLiteral (n, ...args) {
        n.quasis      = this.visitArray(n.quasis, ...args);
        n.expressions = this.visitArray(n.expressions, ...args);
        return n;
    }

    visitTemplateElement (n) { return n; }

    toString () { return 'TreeVisitor'; }
}

export class TransformPass extends TreeVisitor {
    constructor (options) {
        super();
        this.options = options;
    }
}

