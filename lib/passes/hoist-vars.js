/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */
// hoists all vars to the start of the enclosing function, replacing
// any initializer with an assignment expression.  We also take the
// opportunity to convert the vars to lets at this point so by the time
// the LLVMIRVisitory gets to the tree there are only consts and lets
//
// i.e. from
//    {
//       ....
//       var x = 5;
//       ....
//    }
// to
//    {
//       let x;
//       ....
//       x = 5;
//       ....
//    }
//
// we also warn if x was already hoisted (if the decl for it already exists in the toplevel scope)

import * as b from '../ast-builder';
import { TransformPass } from '../node-visitor';
import { Stack } from '../stack-es6';
import Set  from '../set-es6';
import { reportWarning } from '../errors';

function create_empty_declarator (decl_name) {
    return b.variableDeclarator(b.identifier(decl_name), b.undefinedLit());
}

export class HoistVars extends TransformPass {
    constructor (options, filename) {
        super(options);
        this.filename = filename;
        this.scope_stack = new Stack;
    }

    
    visitProgram (n) {
        let vars = new Set;
        this.scope_stack.push ({ func: n, vars: vars });
        n = super.visitProgram(n);
        this.scope_stack.pop();

        if (vars.size === 0) return n;

        let empty_declarators = [];
        vars.forEach( (varname) => empty_declarators.push(create_empty_declarator(varname)));
        n.body.unshift(b.letDeclaration(empty_declarators));
        return n;
    }
    
    visitFunction (n) {
        let vars = new Set;
        this.scope_stack.push ({ func: n, vars: vars });
        n = super.visitFunction(n);
        this.scope_stack.pop();

        if (vars.size === 0) return n;

        let empty_declarators = [];
        vars.forEach( (varname) => empty_declarators.push(create_empty_declarator(varname)));
        n.body.body.unshift(b.letDeclaration(empty_declarators));

        return n;
    }

    visitFor (n) {
        this.skipExpressionStatement = true;
        n.init = this.visit(n.init);
        this.skipExpressionStatement = false;
        n.test = this.visit(n.test);
        n.update = this.visit(n.update);
        n.body = this.visit(n.body);
        return n;
    }
    
    visitForIn (n) {
        if (n.left.type === b.VariableDeclaration) {
            this.scope_stack.top.vars.add(n.left.declarations[0].id.name);
            n.left = b.identifier(n.left.declarations[0].id.name);
        }
        n.right = this.visit(n.right);
        n.body = this.visit(n.body);
        return n;
    }

    visitForOf (n) {
        if (n.left.type === b.VariableDeclaration) {
            this.scope_stack.top.vars.add(n.left.declarations[0].id.name);
            n.left = b.identifier(n.left.declarations[0].id.name);
        }
        n.right = this.visit(n.right);
        n.body = this.visit(n.body);
        return n;
    }
    
    visitVariableDeclaration (n) {
        if (n.kind !== "var") return super.visitVariableDeclaration(n); // we only need to do this for var decls.

        // check to see if there are any initializers, which we'll convert to assignment expressions
        let assignments = [];
        n.declarations.forEach( (decl) => {
            if (decl.init)
                assignments.push(b.assignmentExpression(b.identifier(decl.id.name), "=", this.visit(decl.init)));
        });

        // vars are hoisted to the containing function's toplevel scope
        for (let decl of n.declarations) {
            if (this.scope_stack.top.vars.has(decl.id.name))
                reportWarning(`multiple var declarations for '${decl.id.name}' in this function.`, this.filename, n.loc);
            this.scope_stack.top.vars.add(decl.id.name);
        }

        if (assignments.length === 0) return b.emptyStatement();
        
        let assign_exp;
        // now return the new assignments, which will replace the original variable
        // declaration node.
        if (assignments.length > 1)
            assign_exp = b.sequenceExpression(assignments);
        else
            assign_exp = assignments[0];


        if (this.skipExpressionStatement)
            return assign_exp;
        else
            return b.expressionStatement(assign_exp);
    }
}
