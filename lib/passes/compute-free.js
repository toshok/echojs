/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

// this class really doesn't behave like a normal TreeVisitor, as it modifies the tree in-place.
// XXX reformulate this as a TreeVisitor subclass.

import { ArrayExpression,
         ArrayPattern,
         ArrowFunctionExpression,
         AssignmentExpression,
         BinaryExpression,
         BlockStatement,
         BreakStatement,
         CallExpression,
         CatchClause,
         ClassBody,
         ClassDeclaration,
         ClassExpression,
         ClassHeritage,
         ComprehensionBlock,
         ComprehensionExpression,
         ConditionalExpression,
         ContinueStatement,
         ComputedPropertyKey,
         DebuggerStatement,
         DoWhileStatement,
         EmptyStatement,
         ExportDeclaration,
         ExportBatchSpecifier,
         ExportSpecifier,
         ExpressionStatement,
         ForInStatement,
         ForOfStatement,
         ForStatement,
         FunctionDeclaration,
         FunctionExpression,
         Identifier,
         IfStatement,
         ImportDeclaration,
         ImportSpecifier,
         LabeledStatement,
         Literal,
         LogicalExpression,
         MemberExpression,
         MethodDefinition,
         ModuleDeclaration,
         NewExpression,
         ObjectExpression,
         ObjectPattern,
         Program,
         Property,
         ReturnStatement,
         SequenceExpression,
         SpreadElement,
         SwitchCase,
         SwitchStatement,
         TaggedTemplateExpression,
         TemplateElement,
         TemplateLiteral,
         ThisExpression,
         ThrowStatement,
         TryStatement,
         UnaryExpression,
         UpdateExpression,
         VariableDeclaration,
         VariableDeclarator,
         WhileStatement,
         WithStatement,
         YieldExpression } from '../ast-builder';

import { TransformPass } from '../node-visitor';
import { Set } from '../set-es6';  // we need to rename this type/file, since it's more than a polyfill

export class ComputeFree extends TransformPass {
    constructor () {
        super();
        this.call_free = this.free.bind(this);
        this.setUnion = Set.union;
    }
    
    visit (n) {
        this.free(n);
        return n;
    }

    decl_names (arr) {
        let result = [];
        for (let n of arr) {
            if (n.declarations)
                result = result.concat (n.declarations.map ( (decl) => decl.id.name));
            else if (n.id)
                result.push(n.id.name);
        }
        return new Set(result);
    }

    id_names (arr) {
        return new Set(arr.map ((id) => id.name));
    }

    collect_decls (body) {
        let rv = [];
        body.forEach ( (statement) => {
            if (statement.type === VariableDeclaration || statement.type === FunctionDeclaration)
                rv.push(statement);
        });
        return rv;
    }

    free_blocklike (exp,body) {
        let decls;
        let uses;

        if (body) {
            decls = this.decl_names(this.collect_decls(body));
            uses = this.setUnion.apply(null, body.map(this.call_free));
        }
        else {
            decls = [];
            uses = new Set();
        }
        exp.ejs_decls = decls;
        exp.ejs_free_vars = uses.subtract(decls);
        return exp.ejs_free_vars;
    }

    // TODO: move this into the this.visit method
    free (exp) {
        if (!exp) return new Set;

        // define the properties we'll be filling in below, so that we can make them non-enumerable
        Object.defineProperty(exp, "ejs_decls",     { value: undefined, writable: true, configurable: true });
        Object.defineProperty(exp, "ejs_free_vars", { value: undefined, writable: true, configurable: true });
        
        switch (exp.type) {
        case Program: {
            let decls = this.decl_names(this.collect_decls(exp.body));
            let uses = this.setUnion.apply(null, exp.body.map(this.call_free));
            exp.ejs_decls = decls;
            exp.ejs_free_vars = uses.subtract(decls);
            break;
        }
        case FunctionDeclaration: {
            // this should only happen for the toplevel function we create to wrap the source file
            let param_names = this.id_names(exp.params);
            if (exp.rest) param_names.add(exp.rest.name);
            exp.ejs_free_vars = this.free(exp.body).subtract(param_names);
            exp.ejs_decls = exp.body.ejs_decls.union(param_names);
            break;
        }
        case FunctionExpression: {
            let param_names = this.id_names(exp.params);
            if (exp.rest) param_names.add(exp.rest.name);
            exp.ejs_free_vars = this.free(exp.body).subtract(param_names);
            exp.ejs_decls = param_names.union(exp.body.ejs_decls);
            break;
        }
        case ArrowFunctionExpression: {
            let param_names = this.id_names(exp.params);
            if (exp.rest) param_names.add(exp.rest.name);
            exp.ejs_free_vars = this.free(exp.body).subtract(param_names);
            exp.ejs_decls = param_names.union(exp.body.ejs_decls);
            break;
        }
        case LabeledStatement: {
            exp.ejs_free_vars = this.free(exp.body);
            break;
        }
        case BlockStatement: {
            exp.ejs_free_vars = this.free_blocklike(exp, exp.body);
            break;
        }
        case TryStatement: {
            exp.ejs_free_vars = this.setUnion.apply(null, [this.free(exp.block)].concat(exp.handlers.map(this.call_free)));
            break;
        }
        case CatchClause: {
            let param_set = exp.param && exp.param.name ? new Set [exp.param.name] : new Set;
            exp.ejs_free_vars = this.free(exp.body).subtract(param_set);
            exp.ejs_decls = exp.body.ejs_decls.union(param_set);
            break;
        }
        case VariableDeclaration:
            exp.ejs_free_vars = this.setUnion.apply(null, exp.declarations.map(this.call_free));
            break;
        case VariableDeclarator:
            exp.ejs_free_vars = this.free(exp.init);
            break;
        case ExpressionStatement:
            exp.ejs_free_vars = this.free(exp.expression);
            break;
        case Identifier:
            exp.ejs_free_vars = new Set([exp.name]);
            break;
        case ThrowStatement:
            exp.ejs_free_vars = this.free(exp.argument);
            break;
        case ForStatement:
            exp.ejs_free_vars = this.setUnion.call(null, this.free(exp.init), this.free(exp.test), this.free(exp.update), this.free(exp.body));
            break;
        case ForInStatement:
            exp.ejs_free_vars = this.setUnion.call(null, this.free(exp.left), this.free(exp.right), this.free(exp.body));
            break;
        case ForOfStatement:
            exp.ejs_free_vars = this.setUnion.call(null, this.free(exp.left), this.free(exp.right), this.free(exp.body));
            break;
        case WhileStatement:
            exp.ejs_free_vars = this.setUnion.call(null, this.free(exp.test), this.free(exp.body));
            break;
        case DoWhileStatement:
            exp.ejs_free_vars = this.setUnion.call(null, this.free(exp.test), this.free(exp.body));
            break;
        case SwitchStatement:
            exp.ejs_free_vars = this.setUnion.apply(null, [this.free(exp.discriminant)].concat(exp.cases.map(this.call_free)));
            break;
        case SwitchCase:
            exp.ejs_free_vars = this.free_blocklike(exp, exp.consequent);
            break;
        case EmptyStatement:
            exp.ejs_free_vars = new Set;
            break;
        case BreakStatement:
            exp.ejs_free_vars = new Set;
            break;
        case ContinueStatement:
            exp.ejs_free_vars = new Set;
            break;
        case SpreadElement:
            exp.ejs_free_vars = this.free(exp.argument);
            break;
        case UpdateExpression:
            exp.ejs_free_vars = this.free(exp.argument);
            break;
        case ReturnStatement:
            exp.ejs_free_vars = this.free(exp.argument);
            break;
        case UnaryExpression:
            exp.ejs_free_vars = this.free(exp.argument);
            break;
        case BinaryExpression:
            exp.ejs_free_vars = this.free(exp.left).union(this.free(exp.right));
            break;
        case LogicalExpression:
            exp.ejs_free_vars = this.free(exp.left).union(this.free(exp.right));
            break;
        case MemberExpression:
            exp.ejs_free_vars = this.free(exp.object); // we don't traverse into the property
            break;
        case CallExpression:
            exp.ejs_free_vars = this.setUnion.apply(null, [this.free(exp.callee)].concat(exp.arguments.map(this.call_free)));
            break;
        case NewExpression:
            exp.ejs_free_vars = this.setUnion.apply(null, [this.free(exp.callee)].concat(exp.arguments.map(this.call_free)));
            break;
        case SequenceExpression:
            exp.ejs_free_vars = this.setUnion.apply(null, exp.expressions.map(this.call_free));
            break;
        case ConditionalExpression:
            exp.ejs_free_vars = this.setUnion.call(null, this.free(exp.test), this.free(exp.consequent), this.free(exp.alternate));
            break;
        case TaggedTemplateExpression:
            exp.ejs_free_vars = this.free(exp.quasi);
            break;
        case TemplateLiteral:
            exp.ejs_free_vars = this.setUnion.apply(null, exp.expressions.map(this.call_free));
            break;
        case Literal:
            exp.ejs_free_vars = new Set;
            break;
        case ThisExpression:
            exp.ejs_free_vars = new Set;
            break;
        case ComputedPropertyKey:
            exp.ejs_free_vars = this.free(exp.expression);
            break;
        case Property:
            exp.ejs_free_vars = this.free(exp.value);
            if (exp.key.type === ComputedPropertyKey) {
                // we only do this when the key is computed, or else identifiers show up as free
                exp.ejs_free_vars = exp.ejs_free_vars.union(this.free(exp.key));
            }
            break;
        case ObjectExpression:
            exp.ejs_free_vars = exp.properties.length === 0 ? new Set() : this.setUnion.apply(null, exp.properties.map(this.call_free));
            break;
        case ArrayExpression:
            exp.ejs_free_vars = exp.elements.length === 0 ? new Set() : this.setUnion.apply(null, exp.elements.map(this.call_free));
            break;
        case IfStatement:
            exp.ejs_free_vars = this.setUnion.call(null, this.free(exp.test), this.free(exp.consequent), this.free(exp.alternate));
            break;
        case AssignmentExpression:
            exp.ejs_free_vars = this.free(exp.left).union(this.free(exp.right));
            break;
        case ModuleDeclaration:
            exp.ejs_free_vars = this.free(exp.body);
            break;
        case ExportDeclaration:
            exp.ejs_free_vars = this.free(exp.declaration);
            exp.ejs_decls = exp.declaration.ejs_decls;
            break;
        case ImportDeclaration:
            exp.ejs_decls = new Set(exp.specifiers.map ((specifier) => specifier.id.name));
            // no free vars in an ImportDeclaration
            exp.ejs_free_vars = new Set;
            break;
            
        default:
            throw new Error(`Internal error: unhandled node '${JSON.stringify(exp)}' in free()`);
        }
        return exp.ejs_free_vars;
    }
}
